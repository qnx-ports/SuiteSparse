//------------------------------------------------------------------------------
// SPEX_Cholesky/spex_cholesky_up_triangular_solve: Sparse sym REF tri. solve
//------------------------------------------------------------------------------

// SPEX_Cholesky: (c) 2020-2023, Christopher Lourenco, Jinhao Chen,
// Lorena Mejia Domenzain, Timothy A. Davis, and Erick Moreno-Centeno.
// All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0-or-later or LGPL-3.0-or-later

//------------------------------------------------------------------------------

#define SPEX_FREE_ALL ;

#include "spex_cholesky_internal.h"

/* Purpose: This function performs the symmetric sparse REF triangular solve
 * for the up-looking Cholesky factorization. i,e, LD x = A(1:k-1, k). At the
 * end of this function, the vector x contains the values of the kth row of the
 * integer- preserving matrix L.
 *
 * Input arguments of the function:
 *
 * top_output:      A pointer to the beginning of the nonzero pattern. Undefined
 *                  on input, on output xi[top_output..n] contains the beginning
 *                  of the nonzero pattern.
 *
 * xi:              Nonzero pattern. Undefined on input, on output contains teh
 *                  nonzero pattern of the kth row of L
 *
 * x:               Solution of linear system. Undefined on input, on output
 *                  contains the kth row of L.
 *
 * L:               Lower triangular matrix.
 *
 * A:               Input matrix
 *
 * k:               Current iteration of the algorithm
 *
 * parent:          Elimination tree
 *
 * c:               Column pointers of L but they don't point to the top
 *                  position of each column of L. Instead they point to the
 *                  position on each column where the next value of L will be
 *                  grabbed, since at iteration k we need to grab the kth of L
 *                  in order to not recompute those values.
 *
 * rhos:            Pivot matrix
 *
 * h:               History vector
 */

// Comparison function used for the quicksort in the factorization
// Each iteration of the triangular solve requires that the nonzero pattern
// is sorted prior to numeric operations. This is the helper function for
// c's default qsort
static inline int compare (const void * a, const void * b)
{
//    return ( *(int64_t*)a - *(int64_t*)b );

    int64_t x = (* ((int64_t *) a)) ;
    int64_t y = (* ((int64_t *) b)) ;
    return (x < y ? -1 : ((x == y) ? 0 : 1)) ;
}

SPEX_info spex_cholesky_up_triangular_solve
(
    //Output
    int64_t *top_output,            // On input NULL. On output contains the
                                    // beginning of nonzero pattern
                                    // The nonzero pattern is contained in
                                    // xi[top_output...n-1]
    int64_t *xi,                    // Nonzero pattern vector
    SPEX_matrix x,                  // Solution of system ==> kth row of L
    // Input
    const SPEX_matrix L,            // Partial L matrix
    const SPEX_matrix A,            // Input matrix
    const int64_t k,                // Iteration of algorithm
    const int64_t *parent,          // Elimination tree
    int64_t *c,                     // Column pointers
    const SPEX_matrix rhos,         // sequence of pivots
    int64_t *h                      // History vector
)
{

    SPEX_info info;

    // All inputs are checked by the caller. Here we include
    // asserts as a reminder of the expected data types of the inputs
    ASSERT(L->type == SPEX_MPZ);
    ASSERT(L->kind == SPEX_CSC);
    ASSERT(A->type == SPEX_MPZ);
    ASSERT(A->kind == SPEX_CSC);
    ASSERT(x->type == SPEX_MPZ);
    ASSERT(x->kind == SPEX_DENSE);
    ASSERT(rhos->type == SPEX_MPZ);
    ASSERT(rhos->kind == SPEX_DENSE);

    int64_t j, i, p, m, n = A->n;
    int sgn;
    (*top_output) = n ;
    int64_t top = n ;

    ASSERT (n >= 0) ;
    ASSERT (k >= 0 && k < n) ;
    ASSERT (parent != NULL) ;
    ASSERT (c != NULL) ;
    ASSERT (h != NULL) ;
    ASSERT (xi != NULL) ;

    //--------------------------------------------------------------------------
    // Initialize REF Triangular Solve by getting the nonzero patern of x &&
    // obtaining A(:,k)
    //--------------------------------------------------------------------------
    // Obtain the nonzero pattern of the kth row of L by analyzing the
    // elimination tree of A. The indices of these nonzeros are stored in
    // xi[top..n-1]
    SPEX_CHECK(spex_cholesky_ereach(&top, xi, A, k, parent, c));

    ASSERT (top >= 0 && top <= n) ;

// fprintf (stderr, "Hey: top is %" PRId64 " n is %" PRId64 "\n", top, n) ;
// if (top < 0 || top > n)
// {
//     HERE ;
//     fprintf (stderr, "Hey: top is wierd %" PRId64 " n is %" PRId64 "\n", top, n) ;
//     abort ( ) ;
// }

#if 0
    for (i = top; i < n; i++)
    {
        int64_t j = xi [i] ;
fprintf (stderr, "Hey: i %" PRId64 " j is OK %" PRId64 " n is %" PRId64 "\n", i, j, n) ;
if (j < 0 || j >= n)
{
    HERE ;
    fprintf (stderr, "Hey: j is wierd %" PRId64 " n is %" PRId64 "\n", j, n) ;
    abort ( ) ;
}
    }
#endif

    // Sort the nonzero pattern using quicksort (required by IPGE unlike in GE)
    qsort (&xi[top], n-top, sizeof (int64_t), compare) ;

HERE ;

    // Reset x[i] = 0 for all i in nonzero pattern xi [top..n-1]
    for (i = top; i < n; i++)
    {
        int64_t j = xi [i] ;
        ASSERT (j >= 0 && j <= k) ;

// if (j < 0 || j >= n)
// {
//     HERE ;
//     fprintf (stderr, "Hey: j is wierd %" PRId64 " n is %" PRId64 "\n", j, n) ;
//     abort ( ) ;
// }
// fprintf (stderr, "Hey: i %" PRId64 " j is OK %" PRId64 " n is %" PRId64 "\n", i, j, n) ;

        SPEX_MPZ_SET_UI(x->x.mpz[j],0);
    }

    // Reset value of x[k]. If the matrix is nonsingular, x[k] will
    // be a part of the nonzero pattern and reset in the above loop.
    // However, in some rare cases, the matrix can be singular but x[k]
    // will be nonzero from a previous iteration. Thus, here we reset
    // x[k] to account for this extremely rare case.
    SPEX_MPZ_SET_UI(x->x.mpz[k],0);

    // Reset h[i] = -1 for all i in nonzero pattern
    for (i = top; i < n; i++)
    {
        h[xi[i]] = -1;
    }

    // Set x = A(:,k)
    // Note: The if is needed since the columns of A are allowed to be unsorted.
    for (i = A->p[k]; i < A->p[k+1]; i++)
    {
        if (A->i[i] <= k)
        {
            SPEX_MPZ_SET(x->x.mpz[A->i[i]], A->x.mpz[i]);
        }
    }
HERE

    //--------------------------------------------------------------------------
    // Perform the REF Triangular Solve. Note that, unlike the left-looking
    // Cholesky triangular solve where L is lower trapezoidal, the up-looking L
    // matrix is actually lower triangular; thus this is a true triangular
    // solve.
    //--------------------------------------------------------------------------
    for (p = top; p < n; p++)
    {
HERE
        // Obtain the index of the current nonzero
        j = xi[p];
fprintf (stderr, "==== p %" PRId64 " j %" PRId64 "\n", p, j) ;

        ASSERT (j >= 0 && j <= k) ;

// if (j < 0 || j >= n)
// {
//    HERE ;
//    fprintf (stderr, "Hey: j is wierd %" PRId64 " n is %" PRId64 "\n", j, n) ;
//    abort ( ) ;
// }

        SPEX_MPZ_SGN(&sgn, x->x.mpz[j]);
HERE
        if (sgn == 0) continue;    // If x[j] == 0 no work must be done
HERE

        // Initial history update to finalize x[j] if necessary
        if (h[j] < j-1)
        {
HERE
            // History update x[j]: x[j] = x[j]*rhos[j-1]/rhos[h[j]]
            // x[j] = x[j]*rhos[j-1]
            SPEX_MPZ_MUL(x->x.mpz[j], x->x.mpz[j],
                rhos->x.mpz[j-1]);
HERE
            if (h[j] > -1)
            {
               // x[j] = x[j] / rhos [ h[j] ]
HERE
               SPEX_MPZ_DIVEXACT(x->x.mpz[j], x->x.mpz[j],
                                            rhos->x.mpz[h[j]]);
HERE
            }
        }

        //------------------------------------------------------------------
        // IPGE updates
        //------------------------------------------------------------------
        // ----------- Iterate accross nonzeros in Lij ---------------------
        for (m = L->p[j]+1; m < c[j]; m++)
        {
HERE
            i = L->i[m];            // i value of Lij
            if (i > j && i < k)     // Update all dependent x[i] excluding x[k]
            {
                    /*************** If lij==0 then no update******************/
HERE
                SPEX_MPZ_SGN(&sgn, L->x.mpz[m]);
HERE
                if (sgn == 0) continue;
HERE

                //----------------------------------------------------------
                /************* lij is nonzero, x[i] is zero****************/
                // x[i] = 0 then only perform IPGE update subtraction/division
                //----------------------------------------------------------
                SPEX_MPZ_SGN(&sgn, x->x.mpz[i]);
HERE
                if (sgn == 0)
                {
HERE
                    // First, get the correct value of x[i] = 0 - lij * x[j]
                    SPEX_MPZ_MUL(x->x.mpz[i], L->x.mpz[m],
                                                 x->x.mpz[j]);
HERE
                    SPEX_MPZ_NEG(x->x.mpz[i],x->x.mpz[i]);
HERE
                    // Do a division by the pivot if necessary.
                    if (j >= 1)
                    {
                        // x[i] = x[i] / rho[j-1]
HERE
                        SPEX_MPZ_DIVEXACT(x->x.mpz[i], x->x.mpz[i],
                                                        rhos->x.mpz[j-1]);
HERE
                    }
                    // Update the history value of x[i]
                    h[i] = j;

                }
HERE

                //----------------------------------------------------------
                /************ Both lij and x[i] are nonzero****************/
                // x[i] != 0 --> History & IPGE update on x[i]
                //----------------------------------------------------------
                else
                {
HERE
                    // There is no previous pivot
                    if (j < 1)
                    {
HERE
                        // History update x[i] = x[i]*rhos[0]
                        SPEX_MPZ_MUL(x->x.mpz[i],x->x.mpz[i],
                                                rhos->x.mpz[0]);
HERE
                        // x[i] = x[i] - lij x[j]
                        SPEX_MPZ_SUBMUL(x->x.mpz[i], L->x.mpz[m],
                                                    x->x.mpz[j]);
HERE
                        // Update the history value of x[i]
                        h[i] = j;
                    }
                    // There is a previous pivot
                    else
                    {
HERE
                        // History update if necessary
                        if (h[i] < j - 1)
                        {
HERE
                            // x[i] = x[i] * rhos[j-1]
                            SPEX_MPZ_MUL(x->x.mpz[i],x->x.mpz[i],
                                                     rhos->x.mpz[j-1]);
HERE
                            // Divide by the history pivot only if the history
                            // pivot is not the rho[-1] (which equals 1) (rho[0]
                            // in the 1-based logic of othe IPGE algorithm)
                            if (h[i] > -1)
                            {
HERE
                                // x[i] = x[i] / rho[h[i]]
                                SPEX_MPZ_DIVEXACT(x->x.mpz[i],
                                            x->x.mpz[i],rhos->x.mpz[h[i]]);
HERE
                            }
                        }
                        // ---- IPGE Update :
                        // x[i] = (x[i]*rhos[j] - lij*xj) / rho[j-1]
                        // x[i] = x[i]*rhos[j]
                        SPEX_MPZ_MUL(x->x.mpz[i],x->x.mpz[i],
                                                rhos->x.mpz[j]);
HERE
                        // x[i] = x[i] - lij*xj
                        SPEX_MPZ_SUBMUL(x->x.mpz[i], L->x.mpz[m],
                                                    x->x.mpz[j]);
HERE
                        // x[i] = x[i] / rho[j-1]
                        SPEX_MPZ_DIVEXACT(x->x.mpz[i],x->x.mpz[i],
                                                        rhos->x.mpz[j-1]);
HERE
                        // Entry is up to date;
                        h[i] = j;
                    }
                }
            }
        }

HERE
        // ------ History Update x[k] if necessary -----
        if (h[k] < j - 1)
        {
            // x[k] = x[k] * rho[j-1]
            SPEX_MPZ_MUL(x->x.mpz[k],x->x.mpz[k],rhos->x.mpz[j-1]);
            // Divide by the history pivot only if the history pivot is not the
            // rho[-1] (which equals 1) (rho[0] in the 1-based logic of the
            // IPGE algorithm)
            if (h[k] > -1)
            {
                // x[k] = x[k] / rho[h[k]]
                SPEX_MPZ_DIVEXACT(x->x.mpz[k],x->x.mpz[k],
                                              rhos->x.mpz[h[k]]);
            }
        }
HERE

        // ---- IPGE Update x[k] = (x[k]*rhos[j] - xj*xj) / rho[j-1] ------
        // x[k] = x[k] * rho[j]
        SPEX_MPZ_MUL(x->x.mpz[k],x->x.mpz[k],rhos->x.mpz[j]);
        // x[k] = x[k] - xj*xj
        SPEX_MPZ_SUBMUL(x->x.mpz[k], x->x.mpz[j], x->x.mpz[j]);
        // Only divide by previous pivot if the previous pivot is not 1 (which
        // is always the case in the first IPGE iteration)
HERE
        if (j > 0)
        {
            // x[k] = x[k] / rho[j-1]
HERE
// THIS FAILS with SPEX_PANIC: rhos[j] is zero,
// then SPEX segfaults
fprintf (stderr, "------ exact divide by rhos[j]: j = %" PRId64" of n %" PRId64 "\n", j, n) ;
            SPEX_MPZ_DIVEXACT(x->x.mpz[k],x->x.mpz[k],
                                            rhos->x.mpz[j-1]);
HERE
        }
        // Entry is up to date;
        h[k] = j;
    }
HERE

    //----------------------------------------------------------
    // At this point, x[k] has been updated throughout the
    // triangular solve. The last step is to make sure x[k]
    // has its correct final value. Thus, a final history
    // update is done to x[k] if necessary
    //----------------------------------------------------------
HERE
    if (h[k] < k-1)
    {
        // x[k] = x[k] * rhos[k-1]
HERE
        SPEX_MPZ_MUL(x->x.mpz[k], x->x.mpz[k], rhos->x.mpz[k-1]);
        // Only divide by previous pivot if the previous pivot is not 1 (which
        // is always the case in the first IPGE iteration)
        if (h[k] > -1)
        {
HERE
            // x[k] = x[k] / rhos[h[k]]
            SPEX_MPZ_DIVEXACT(x->x.mpz[k], x->x.mpz[k],
                                           rhos->x.mpz[ h[k]]);
HERE
        }
    }
    // Output the top of the nonzero pattern
HERE
    (*top_output) = top;
HERE
    return SPEX_OK;
}
