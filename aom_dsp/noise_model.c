/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/noise_model.h"
#include "aom_dsp/noise_util.h"
#include "aom_mem/aom_mem.h"
#include "av1/encoder/mathutils.h"

#define kLowPolyNumParams 3

static const int kMaxLag = 4;

// Defines a function that can be used to obtain the mean of a block for the
// provided data type (uint8_t, or uint16_t)
#define GET_BLOCK_MEAN(INT_TYPE, suffix)                                    \
  static double get_block_mean_##suffix(const INT_TYPE *data, int w, int h, \
                                        int stride, int x_o, int y_o,       \
                                        int block_size) {                   \
    const int max_h = AOMMIN(h - y_o, block_size);                          \
    const int max_w = AOMMIN(w - x_o, block_size);                          \
    double block_mean = 0;                                                  \
    for (int y = 0; y < max_h; ++y) {                                       \
      for (int x = 0; x < max_w; ++x) {                                     \
        block_mean += data[(y_o + y) * stride + x_o + x];                   \
      }                                                                     \
    }                                                                       \
    return block_mean / (max_w * max_h);                                    \
  }

GET_BLOCK_MEAN(uint8_t, lowbd);
GET_BLOCK_MEAN(uint16_t, highbd);

inline static double get_block_mean(const uint8_t *data, int w, int h,
                                    int stride, int x_o, int y_o,
                                    int block_size, int use_highbd) {
  if (use_highbd)
    return get_block_mean_highbd((const uint16_t *)data, w, h, stride, x_o, y_o,
                                 block_size);
  return get_block_mean_lowbd(data, w, h, stride, x_o, y_o, block_size);
}

static void equation_system_clear(aom_equation_system_t *eqns) {
  const int n = eqns->n;
  memset(eqns->A, 0, sizeof(*eqns->A) * n * n);
  memset(eqns->x, 0, sizeof(*eqns->x) * n);
  memset(eqns->b, 0, sizeof(*eqns->b) * n);
}

static void equation_system_copy(aom_equation_system_t *dst,
                                 const aom_equation_system_t *src) {
  const int n = dst->n;
  memcpy(dst->A, src->A, sizeof(*dst->A) * n * n);
  memcpy(dst->x, src->x, sizeof(*dst->x) * n);
  memcpy(dst->b, src->b, sizeof(*dst->b) * n);
}

static int equation_system_init(aom_equation_system_t *eqns, int n) {
  eqns->A = (double *)aom_malloc(sizeof(*eqns->A) * n * n);
  eqns->b = (double *)aom_malloc(sizeof(*eqns->b) * n);
  eqns->x = (double *)aom_malloc(sizeof(*eqns->x) * n);
  eqns->n = n;
  if (!eqns->A || !eqns->b || !eqns->x) {
    fprintf(stderr, "Failed to allocate system of equations of size %d\n", n);
    aom_free(eqns->A);
    aom_free(eqns->b);
    aom_free(eqns->x);
    memset(eqns, 0, sizeof(*eqns));
    return 0;
  }
  equation_system_clear(eqns);
  return 1;
}

static int equation_system_solve(aom_equation_system_t *eqns) {
  const int n = eqns->n;
  double *b = (double *)aom_malloc(sizeof(*b) * n);
  double *A = (double *)aom_malloc(sizeof(*A) * n * n);
  int ret = 0;
  if (A == NULL || b == NULL) {
    fprintf(stderr, "Unable to allocate temp values of size %dx%d\n", n, n);
    aom_free(b);
    aom_free(A);
    return 0;
  }
  memcpy(A, eqns->A, sizeof(*eqns->A) * n * n);
  memcpy(b, eqns->b, sizeof(*eqns->b) * n);
  ret = linsolve(n, A, eqns->n, b, eqns->x);
  aom_free(b);
  aom_free(A);

  if (ret == 0) {
    fprintf(stderr, "Solving %dx%d system failed!\n", n, n);
    return 0;
  }
  return 1;
}

static void equation_system_add(aom_equation_system_t *dest,
                                aom_equation_system_t *src) {
  const int n = dest->n;
  int i, j;
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n; ++j) {
      dest->A[i * n + j] += src->A[i * n + j];
    }
    dest->b[i] += src->b[i];
  }
}

static void equation_system_free(aom_equation_system_t *eqns) {
  if (!eqns) return;
  aom_free(eqns->A);
  aom_free(eqns->b);
  aom_free(eqns->x);
  memset(eqns, 0, sizeof(*eqns));
}

static void noise_strength_solver_clear(aom_noise_strength_solver_t *solver) {
  equation_system_clear(&solver->eqns);
  solver->num_equations = 0;
  solver->total = 0;
}

static void noise_strength_solver_add(aom_noise_strength_solver_t *dest,
                                      aom_noise_strength_solver_t *src) {
  equation_system_add(&dest->eqns, &src->eqns);
  dest->num_equations += src->num_equations;
  dest->total += src->total;
}

// Return the number of coefficients required for the given parameters
static int num_coeffs(const aom_noise_model_params_t params) {
  const int n = 2 * params.lag + 1;
  switch (params.shape) {
    case AOM_NOISE_SHAPE_DIAMOND: return params.lag * (params.lag + 1);
    case AOM_NOISE_SHAPE_SQUARE: return (n * n) / 2;
  }
  return 0;
}

static int noise_state_init(aom_noise_state_t *state, int n, int bit_depth) {
  const int kNumBins = 20;
  if (!equation_system_init(&state->eqns, n)) {
    fprintf(stderr, "Failed initialization noise state with size %d\n", n);
    return 0;
  }
  return aom_noise_strength_solver_init(&state->strength_solver, kNumBins,
                                        bit_depth);
}

static void set_chroma_coefficient_fallback_soln(aom_equation_system_t *eqns) {
  const double kTolerance = 1e-6;
  const int last = eqns->n - 1;
  // Set all of the AR coefficients to zero, but try to solve for correlation
  // with the luma channel
  memset(eqns->x, 0, sizeof(*eqns->x) * eqns->n);
  if (fabs(eqns->A[last * eqns->n + last]) > kTolerance) {
    eqns->x[last] = eqns->b[last] / eqns->A[last * eqns->n + last];
  }
}

int aom_noise_strength_lut_init(aom_noise_strength_lut_t *lut, int num_points) {
  if (!lut) return 0;
  lut->points = (double(*)[2])aom_malloc(num_points * sizeof(*lut->points));
  if (!lut->points) return 0;
  lut->num_points = num_points;
  memset(lut->points, 0, sizeof(*lut->points) * num_points);
  return 1;
}

void aom_noise_strength_lut_free(aom_noise_strength_lut_t *lut) {
  if (!lut) return;
  aom_free(lut->points);
  memset(lut, 0, sizeof(*lut));
}

double aom_noise_strength_lut_eval(const aom_noise_strength_lut_t *lut,
                                   double x) {
  int i = 0;
  // Constant extrapolation for x <  x_0.
  if (x < lut->points[0][0]) return lut->points[0][1];
  for (i = 0; i < lut->num_points - 1; ++i) {
    if (x >= lut->points[i][0] && x <= lut->points[i + 1][0]) {
      const double a =
          (x - lut->points[i][0]) / (lut->points[i + 1][0] - lut->points[i][0]);
      return lut->points[i + 1][1] * a + lut->points[i][1] * (1.0 - a);
    }
  }
  // Constant extrapolation for x > x_{n-1}
  return lut->points[lut->num_points - 1][1];
}

static double noise_strength_solver_get_bin_index(
    const aom_noise_strength_solver_t *solver, double value) {
  const double val =
      fclamp(value, solver->min_intensity, solver->max_intensity);
  const double range = solver->max_intensity - solver->min_intensity;
  return (solver->num_bins - 1) * (val - solver->min_intensity) / range;
}

void aom_noise_strength_solver_add_measurement(
    aom_noise_strength_solver_t *solver, double block_mean, double noise_std) {
  const double bin = noise_strength_solver_get_bin_index(solver, block_mean);
  const int bin_i0 = (int)floor(bin);
  const int bin_i1 = AOMMIN(solver->num_bins - 1, bin_i0 + 1);
  const double a = bin - bin_i0;
  const int n = solver->num_bins;
  solver->eqns.A[bin_i0 * n + bin_i0] += (1.0 - a) * (1.0 - a);
  solver->eqns.A[bin_i1 * n + bin_i0] += a * (1.0 - a);
  solver->eqns.A[bin_i1 * n + bin_i1] += a * a;
  solver->eqns.A[bin_i0 * n + bin_i1] += a * (1.0 - a);
  solver->eqns.b[bin_i0] += (1.0 - a) * noise_std;
  solver->eqns.b[bin_i1] += a * noise_std;
  solver->total += noise_std;
  solver->num_equations++;
}

int aom_noise_strength_solver_solve(aom_noise_strength_solver_t *solver) {
  // Add regularization proportional to the number of constraints
  const int n = solver->num_bins;
  const double kAlpha = 2.0 * (double)(solver->num_equations) / n;
  int result = 0;
  double mean = 0;

  // Do this in a non-destructive manner so it is not confusing to the caller
  double *old_A = solver->eqns.A;
  double *A = (double *)aom_malloc(sizeof(*A) * n * n);
  if (!A) {
    fprintf(stderr, "Unable to allocate copy of A\n");
    return 0;
  }
  memcpy(A, old_A, sizeof(*A) * n * n);

  for (int i = 0; i < n; ++i) {
    const int i_lo = AOMMAX(0, i - 1);
    const int i_hi = AOMMIN(n - 1, i + 1);
    A[i * n + i_lo] -= kAlpha;
    A[i * n + i] += 2 * kAlpha;
    A[i * n + i_hi] -= kAlpha;
  }

  // Small regularization to give average noise strength
  mean = solver->total / solver->num_equations;
  for (int i = 0; i < n; ++i) {
    A[i * n + i] += 1.0 / 8192.;
    solver->eqns.b[i] += mean / 8192.;
  }
  solver->eqns.A = A;
  result = equation_system_solve(&solver->eqns);
  solver->eqns.A = old_A;

  aom_free(A);
  return result;
}

int aom_noise_strength_solver_init(aom_noise_strength_solver_t *solver,
                                   int num_bins, int bit_depth) {
  if (!solver) return 0;
  memset(solver, 0, sizeof(*solver));
  solver->num_bins = num_bins;
  solver->min_intensity = 0;
  solver->max_intensity = (1 << bit_depth) - 1;
  solver->total = 0;
  solver->num_equations = 0;
  return equation_system_init(&solver->eqns, num_bins);
}

void aom_noise_strength_solver_free(aom_noise_strength_solver_t *solver) {
  if (!solver) return;
  equation_system_free(&solver->eqns);
}

double aom_noise_strength_solver_get_center(
    const aom_noise_strength_solver_t *solver, int i) {
  const double range = solver->max_intensity - solver->min_intensity;
  const int n = solver->num_bins;
  return ((double)i) / (n - 1) * range + solver->min_intensity;
}

// Computes the residual if a point were to be removed from the lut. This is
// calculated as the area between the output of the solver and the line segment
// that would be formed between [x_{i - 1}, x_{i + 1}).
static void update_piecewise_linear_residual(
    const aom_noise_strength_solver_t *solver,
    const aom_noise_strength_lut_t *lut, double *residual, int start, int end) {
  const double dx = 255. / solver->num_bins;
  for (int i = AOMMAX(start, 1); i < AOMMIN(end, lut->num_points - 1); ++i) {
    const int lower = AOMMAX(0, (int)floor(noise_strength_solver_get_bin_index(
                                    solver, lut->points[i - 1][0])));
    const int upper = AOMMIN(solver->num_bins - 1,
                             (int)ceil(noise_strength_solver_get_bin_index(
                                 solver, lut->points[i + 1][0])));
    double r = 0;
    for (int j = lower; j <= upper; ++j) {
      const double x = aom_noise_strength_solver_get_center(solver, j);
      if (x < lut->points[i - 1][0]) continue;
      if (x >= lut->points[i + 1][0]) continue;
      const double y = solver->eqns.x[j];
      const double a = (x - lut->points[i - 1][0]) /
                       (lut->points[i + 1][0] - lut->points[i - 1][0]);
      const double estimate_y =
          lut->points[i - 1][1] * (1.0 - a) + lut->points[i + 1][1] * a;
      r += fabs(y - estimate_y);
    }
    residual[i] = r * dx;
  }
}

int aom_noise_strength_solver_fit_piecewise(
    const aom_noise_strength_solver_t *solver, int max_output_points,
    aom_noise_strength_lut_t *lut) {
  // The tolerance is normalized to be give consistent results between
  // different bit-depths.
  const double kTolerance = solver->max_intensity * 0.00625 / 255.0;
  if (!aom_noise_strength_lut_init(lut, solver->num_bins)) {
    fprintf(stderr, "Failed to init lut\n");
    return 0;
  }
  for (int i = 0; i < solver->num_bins; ++i) {
    lut->points[i][0] = aom_noise_strength_solver_get_center(solver, i);
    lut->points[i][1] = solver->eqns.x[i];
  }
  if (max_output_points < 0) {
    max_output_points = solver->num_bins;
  }

  double *residual = aom_malloc(solver->num_bins * sizeof(*residual));
  memset(residual, 0, sizeof(*residual) * solver->num_bins);

  update_piecewise_linear_residual(solver, lut, residual, 0, solver->num_bins);

  // Greedily remove points if there are too many or if it doesn't hurt local
  // approximation (never remove the end points)
  while (lut->num_points > 2) {
    int min_index = 1;
    for (int j = 1; j < lut->num_points - 1; ++j) {
      if (residual[j] < residual[min_index]) {
        min_index = j;
      }
    }
    const double dx =
        lut->points[min_index + 1][0] - lut->points[min_index - 1][0];
    const double avg_residual = residual[min_index] / dx;
    if (lut->num_points <= max_output_points && avg_residual > kTolerance) {
      break;
    }

    const int num_remaining = lut->num_points - min_index - 1;
    memmove(lut->points + min_index, lut->points + min_index + 1,
            sizeof(lut->points[0]) * num_remaining);
    lut->num_points--;

    update_piecewise_linear_residual(solver, lut, residual, min_index - 1,
                                     min_index + 1);
  }
  aom_free(residual);
  return 1;
}

int aom_flat_block_finder_init(aom_flat_block_finder_t *block_finder,
                               int block_size, int bit_depth, int use_highbd) {
  const int n = block_size * block_size;
  aom_equation_system_t eqns;
  double *AtA_inv = 0;
  double *A = 0;
  int x = 0, y = 0, i = 0, j = 0;
  if (!equation_system_init(&eqns, kLowPolyNumParams)) {
    fprintf(stderr, "Failed to init equation system for block_size=%d\n",
            block_size);
    return 0;
  }

  AtA_inv = (double *)aom_malloc(kLowPolyNumParams * kLowPolyNumParams *
                                 sizeof(*AtA_inv));
  A = (double *)aom_malloc(kLowPolyNumParams * n * sizeof(*A));
  if (AtA_inv == NULL || A == NULL) {
    fprintf(stderr, "Failed to alloc A or AtA_inv for block_size=%d\n",
            block_size);
    aom_free(AtA_inv);
    aom_free(A);
    equation_system_free(&eqns);
    return 0;
  }

  block_finder->A = A;
  block_finder->AtA_inv = AtA_inv;
  block_finder->block_size = block_size;
  block_finder->normalization = (1 << bit_depth) - 1;
  block_finder->use_highbd = use_highbd;

  for (y = 0; y < block_size; ++y) {
    const double yd = ((double)y - block_size / 2.) / (block_size / 2.);
    for (x = 0; x < block_size; ++x) {
      const double xd = ((double)x - block_size / 2.) / (block_size / 2.);
      const double coords[3] = { yd, xd, 1 };
      const int row = y * block_size + x;
      A[kLowPolyNumParams * row + 0] = yd;
      A[kLowPolyNumParams * row + 1] = xd;
      A[kLowPolyNumParams * row + 2] = 1;

      for (i = 0; i < kLowPolyNumParams; ++i) {
        for (j = 0; j < kLowPolyNumParams; ++j) {
          eqns.A[kLowPolyNumParams * i + j] += coords[i] * coords[j];
        }
      }
    }
  }

  // Lazy inverse using existing equation solver.
  for (i = 0; i < kLowPolyNumParams; ++i) {
    memset(eqns.b, 0, sizeof(*eqns.b) * kLowPolyNumParams);
    eqns.b[i] = 1;
    equation_system_solve(&eqns);

    for (j = 0; j < kLowPolyNumParams; ++j) {
      AtA_inv[j * kLowPolyNumParams + i] = eqns.x[j];
    }
  }
  equation_system_free(&eqns);
  return 1;
}

void aom_flat_block_finder_free(aom_flat_block_finder_t *block_finder) {
  if (!block_finder) return;
  aom_free(block_finder->A);
  aom_free(block_finder->AtA_inv);
  memset(block_finder, 0, sizeof(*block_finder));
}

void aom_flat_block_finder_extract_block(
    const aom_flat_block_finder_t *block_finder, const uint8_t *const data,
    int w, int h, int stride, int offsx, int offsy, double *plane,
    double *block) {
  const int block_size = block_finder->block_size;
  const int n = block_size * block_size;
  const double *A = block_finder->A;
  const double *AtA_inv = block_finder->AtA_inv;
  double plane_coords[kLowPolyNumParams];
  double AtA_inv_b[kLowPolyNumParams];
  int xi, yi, i;

  if (block_finder->use_highbd) {
    const uint16_t *const data16 = (const uint16_t *const)data;
    for (yi = 0; yi < block_size; ++yi) {
      const int y = clamp(offsy + yi, 0, h - 1);
      for (xi = 0; xi < block_size; ++xi) {
        const int x = clamp(offsx + xi, 0, w - 1);
        block[yi * block_size + xi] =
            ((double)data16[y * stride + x]) / block_finder->normalization;
      }
    }
  } else {
    for (yi = 0; yi < block_size; ++yi) {
      const int y = clamp(offsy + yi, 0, h - 1);
      for (xi = 0; xi < block_size; ++xi) {
        const int x = clamp(offsx + xi, 0, w - 1);
        block[yi * block_size + xi] =
            ((double)data[y * stride + x]) / block_finder->normalization;
      }
    }
  }
  multiply_mat(block, A, AtA_inv_b, 1, n, kLowPolyNumParams);
  multiply_mat(AtA_inv, AtA_inv_b, plane_coords, kLowPolyNumParams,
               kLowPolyNumParams, 1);
  multiply_mat(A, plane_coords, plane, n, kLowPolyNumParams, 1);

  for (i = 0; i < n; ++i) {
    block[i] -= plane[i];
  }
}

int aom_flat_block_finder_run(const aom_flat_block_finder_t *block_finder,
                              const uint8_t *const data, int w, int h,
                              int stride, uint8_t *flat_blocks) {
  const int block_size = block_finder->block_size;
  const int n = block_size * block_size;
  const double kTraceThreshold = 0.1 / (32 * 32);
  const double kRatioThreshold = 1.2;
  const double kNormThreshold = 0.05 / (32 * 32);
  const double kVarThreshold = 0.005 / (double)n;
  const int num_blocks_w = (w + block_size - 1) / block_size;
  const int num_blocks_h = (h + block_size - 1) / block_size;
  int num_flat = 0;
  int bx = 0, by = 0;
  double *plane = (double *)aom_malloc(n * sizeof(*plane));
  double *block = (double *)aom_malloc(n * sizeof(*block));
  if (plane == NULL || block == NULL) {
    fprintf(stderr, "Failed to allocate memory for block of size %d\n", n);
    aom_free(plane);
    aom_free(block);
    return -1;
  }

  for (by = 0; by < num_blocks_h; ++by) {
    for (bx = 0; bx < num_blocks_w; ++bx) {
      // Compute gradient covariance matrix.
      double Gxx = 0, Gxy = 0, Gyy = 0;
      double var = 0;
      double mean = 0;
      int xi, yi;
      aom_flat_block_finder_extract_block(block_finder, data, w, h, stride,
                                          bx * block_size, by * block_size,
                                          plane, block);

      for (yi = 1; yi < block_size - 1; ++yi) {
        for (xi = 1; xi < block_size - 1; ++xi) {
          const double gx = (block[yi * block_size + xi + 1] -
                             block[yi * block_size + xi - 1]) /
                            2;
          const double gy = (block[yi * block_size + xi + block_size] -
                             block[yi * block_size + xi - block_size]) /
                            2;
          Gxx += gx * gx;
          Gxy += gx * gy;
          Gyy += gy * gy;

          mean += block[yi * block_size + xi];
          var += block[yi * block_size + xi] * block[yi * block_size + xi];
        }
      }
      mean /= (block_size - 2) * (block_size - 2);

      // Normalize gradients by block_size.
      Gxx /= (block_size - 2) * (block_size - 2);
      Gxy /= (block_size - 2) * (block_size - 2);
      Gyy /= (block_size - 2) * (block_size - 2);
      var = var / (block_size - 2) * (block_size - 2) - mean * mean;

      {
        const double trace = Gxx + Gyy;
        const double det = Gxx * Gyy - Gxy * Gxy;
        const double e1 = (trace + sqrt(trace * trace - 4 * det)) / 2.;
        const double e2 = (trace - sqrt(trace * trace - 4 * det)) / 2.;
        const double norm = sqrt(Gxx * Gxx + Gxy * Gxy * 2 + Gyy * Gyy);
        const int is_flat = (trace < kTraceThreshold) &&
                            (e1 / AOMMAX(e2, 1e-8) < kRatioThreshold) &&
                            norm < kNormThreshold && var > kVarThreshold;
        flat_blocks[by * num_blocks_w + bx] = is_flat ? 255 : 0;
        num_flat += is_flat;
      }
    }
  }

  aom_free(block);
  aom_free(plane);
  return num_flat;
}

int aom_noise_model_init(aom_noise_model_t *model,
                         const aom_noise_model_params_t params) {
  const int n = num_coeffs(params);
  const int lag = params.lag;
  const int bit_depth = params.bit_depth;
  int x = 0, y = 0, i = 0, c = 0;

  memset(model, 0, sizeof(*model));
  if (params.lag < 1) {
    fprintf(stderr, "Invalid noise param: lag = %d must be >= 1\n", params.lag);
    return 0;
  }
  if (params.lag > kMaxLag) {
    fprintf(stderr, "Invalid noise param: lag = %d must be <= %d\n", params.lag,
            kMaxLag);
    return 0;
  }

  memcpy(&model->params, &params, sizeof(params));
  for (c = 0; c < 3; ++c) {
    if (!noise_state_init(&model->combined_state[c], n + (c > 0), bit_depth)) {
      fprintf(stderr, "Failed to allocate noise state for channel %d\n", c);
      aom_noise_model_free(model);
      return 0;
    }
    if (!noise_state_init(&model->latest_state[c], n + (c > 0), bit_depth)) {
      fprintf(stderr, "Failed to allocate noise state for channel %d\n", c);
      aom_noise_model_free(model);
      return 0;
    }
  }
  model->n = n;
  model->coords = (int(*)[2])aom_malloc(sizeof(*model->coords) * n);

  for (y = -lag; y <= 0; ++y) {
    const int max_x = y == 0 ? -1 : lag;
    for (x = -lag; x <= max_x; ++x) {
      switch (params.shape) {
        case AOM_NOISE_SHAPE_DIAMOND:
          if (abs(x) <= y + lag) {
            model->coords[i][0] = x;
            model->coords[i][1] = y;
            ++i;
          }
          break;
        case AOM_NOISE_SHAPE_SQUARE:
          model->coords[i][0] = x;
          model->coords[i][1] = y;
          ++i;
          break;
        default:
          fprintf(stderr, "Invalid shape\n");
          aom_noise_model_free(model);
          return 0;
      }
    }
  }
  assert(i == n);
  return 1;
}

void aom_noise_model_free(aom_noise_model_t *model) {
  int c = 0;
  if (!model) return;

  aom_free(model->coords);
  for (c = 0; c < 3; ++c) {
    equation_system_free(&model->latest_state[c].eqns);
    equation_system_free(&model->combined_state[c].eqns);

    equation_system_free(&model->latest_state[c].strength_solver.eqns);
    equation_system_free(&model->combined_state[c].strength_solver.eqns);
  }
  memset(model, 0, sizeof(*model));
}

// Extracts the neighborhood defined by coords around point (x, y) from
// the difference between the data and denoised images. Also extracts the
// entry (possibly downsampled) for (x, y) in the alt_data (e.g., luma).
#define EXTRACT_AR_ROW(INT_TYPE, suffix)                                   \
  static double extract_ar_row_##suffix(                                   \
      int(*coords)[2], int num_coords, const INT_TYPE *const data,         \
      const INT_TYPE *const denoised, int stride, int sub_log2[2],         \
      const INT_TYPE *const alt_data, const INT_TYPE *const alt_denoised,  \
      int alt_stride, int x, int y, double *buffer) {                      \
    for (int i = 0; i < num_coords; ++i) {                                 \
      const int x_i = x + coords[i][0], y_i = y + coords[i][1];            \
      buffer[i] =                                                          \
          (double)data[y_i * stride + x_i] - denoised[y_i * stride + x_i]; \
    }                                                                      \
    const double val =                                                     \
        (double)data[y * stride + x] - denoised[y * stride + x];           \
                                                                           \
    if (alt_data && alt_denoised) {                                        \
      double avg_data = 0, avg_denoised = 0;                               \
      int num_samples = 0;                                                 \
      for (int dy_i = 0; dy_i < (1 << sub_log2[1]); dy_i++) {              \
        const int y_up = (y << sub_log2[1]) + dy_i;                        \
        for (int dx_i = 0; dx_i < (1 << sub_log2[0]); dx_i++) {            \
          const int x_up = (x << sub_log2[0]) + dx_i;                      \
          avg_data += alt_data[y_up * alt_stride + x_up];                  \
          avg_denoised += alt_denoised[y_up * alt_stride + x_up];          \
          num_samples++;                                                   \
        }                                                                  \
      }                                                                    \
      buffer[num_coords] = (avg_data - avg_denoised) / num_samples;        \
    }                                                                      \
    return val;                                                            \
  }

// Defines a function that can be used to extract the residual of the fitted
// AR coefficients and the noise observed point (x, y).
#define EVAL_AR_RESIDUAL(INT_TYPE, suffix)                                    \
  static double eval_ar_residual_##suffix(                                    \
      int(*coords)[2], const double *coeffs, int num_coords,                  \
      const INT_TYPE *const data, const INT_TYPE *const denoised, int stride, \
      int sub_log2[2], const INT_TYPE *const alt_data,                        \
      const INT_TYPE *const alt_denoised, int alt_stride, int x, int y) {     \
    const double actual =                                                     \
        (double)data[y * stride + x] - denoised[y * stride + x];              \
    double sum = 0;                                                           \
    for (int i = 0; i < num_coords; ++i) {                                    \
      const int x_i = (x + coords[i][0]), y_i = (y + coords[i][1]);           \
      sum += coeffs[i] * ((double)(data[y_i * stride + x_i]) -                \
                          denoised[y_i * stride + x_i]);                      \
    }                                                                         \
    if (alt_data && alt_denoised) {                                           \
      double avg_data = 0, avg_denoised = 0;                                  \
      int n = 0;                                                              \
      for (int dy_i = 0; dy_i < (1 << sub_log2[1]); dy_i++) {                 \
        const int y_up = (y << sub_log2[1]) + dy_i;                           \
        for (int dx_i = 0; dx_i < (1 << sub_log2[0]); dx_i++) {               \
          const int x_up = (x << sub_log2[0]) + dx_i;                         \
          avg_data += alt_data[y_up * alt_stride + x_up];                     \
          avg_denoised += alt_denoised[y_up * alt_stride + x_up];             \
          n++;                                                                \
        }                                                                     \
      }                                                                       \
      sum += coeffs[num_coords] * (avg_data - avg_denoised) / n;              \
    }                                                                         \
    return sum - actual;                                                      \
  }

EXTRACT_AR_ROW(uint8_t, lowbd);
EXTRACT_AR_ROW(uint16_t, highbd);

EVAL_AR_RESIDUAL(uint8_t, lowbd);
EVAL_AR_RESIDUAL(uint16_t, highbd);

static int add_block_observations(
    aom_noise_model_t *noise_model, int c, const uint8_t *const data,
    const uint8_t *const denoised, int w, int h, int stride, int sub_log2[2],
    const uint8_t *const alt_data, const uint8_t *const alt_denoised,
    int alt_stride, const uint8_t *const flat_blocks, int block_size,
    int num_blocks_w, int num_blocks_h) {
  const int lag = noise_model->params.lag;
  const int num_coords = noise_model->n;
  const double normalization = (1 << noise_model->params.bit_depth) - 1;
  double *A = noise_model->latest_state[c].eqns.A;
  double *b = noise_model->latest_state[c].eqns.b;
  double *buffer = (double *)aom_malloc(sizeof(*buffer) * (num_coords + 1));
  const int n = noise_model->latest_state[c].eqns.n;

  if (!buffer) {
    fprintf(stderr, "Unable to allocate buffer of size %d\n", num_coords + 1);
    return 0;
  }
  for (int by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * (block_size >> sub_log2[1]);
    for (int bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * (block_size >> sub_log2[0]);
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      int y_start =
          (by > 0 && flat_blocks[(by - 1) * num_blocks_w + bx]) ? 0 : lag;
      int x_start =
          (bx > 0 && flat_blocks[by * num_blocks_w + bx - 1]) ? 0 : lag;
      int y_end = AOMMIN((h >> sub_log2[1]) - by * (block_size >> sub_log2[1]),
                         block_size >> sub_log2[1]);
      int x_end = AOMMIN(
          (w >> sub_log2[0]) - bx * (block_size >> sub_log2[0]) - lag,
          (bx + 1 < num_blocks_w && flat_blocks[by * num_blocks_w + bx + 1])
              ? (block_size >> sub_log2[0])
              : ((block_size >> sub_log2[0]) - lag));
      for (int y = y_start; y < y_end; ++y) {
        for (int x = x_start; x < x_end; ++x) {
          const double val =
              noise_model->params.use_highbd
                  ? extract_ar_row_highbd(noise_model->coords, num_coords,
                                          (const uint16_t *const)data,
                                          (const uint16_t *const)denoised,
                                          stride, sub_log2,
                                          (const uint16_t *const)alt_data,
                                          (const uint16_t *const)alt_denoised,
                                          alt_stride, x + x_o, y + y_o, buffer)
                  : extract_ar_row_lowbd(noise_model->coords, num_coords, data,
                                         denoised, stride, sub_log2, alt_data,
                                         alt_denoised, alt_stride, x + x_o,
                                         y + y_o, buffer);
          for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
              A[i * n + j] +=
                  (buffer[i] * buffer[j]) / (normalization * normalization);
            }
            b[i] += (buffer[i] * val) / (normalization * normalization);
          }
        }
      }
    }
  }
  aom_free(buffer);
  return 1;
}

static void add_noise_std_observations(
    aom_noise_model_t *noise_model, int c, const double *coeffs,
    const uint8_t *const data, const uint8_t *const denoised, int w, int h,
    int stride, int sub_log2[2], const uint8_t *const alt_data,
    const uint8_t *const alt_denoised, int alt_stride,
    const uint8_t *const flat_blocks, int block_size, int num_blocks_w,
    int num_blocks_h) {
  const int lag = noise_model->params.lag;
  const int num_coords = noise_model->n;
  aom_noise_strength_solver_t *noise_strength_solver =
      &noise_model->latest_state[c].strength_solver;

  for (int by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * (block_size >> sub_log2[1]);
    for (int bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * (block_size >> sub_log2[0]);
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      const double block_mean = get_block_mean(
          alt_data ? alt_data : data, w, h, alt_data ? alt_stride : stride,
          x_o << sub_log2[0], y_o << sub_log2[1], block_size,
          noise_model->params.use_highbd);
      double noise_var = 0;
      int num_samples_in_block = 0;
      const int y_start =
          (by > 0 && flat_blocks[(by - 1) * num_blocks_w + bx]) ? 0 : lag;
      const int x_start =
          (bx > 0 && flat_blocks[by * num_blocks_w + bx - 1]) ? 0 : lag;
      const int y_end =
          AOMMIN((h >> sub_log2[1]) - by * (block_size >> sub_log2[1]),
                 block_size >> sub_log2[1]);
      const int x_end = AOMMIN(
          (w >> sub_log2[0]) - bx * (block_size >> sub_log2[0]) - lag,
          (bx + 1 < num_blocks_w && flat_blocks[by * num_blocks_w + bx + 1])
              ? (block_size >> sub_log2[0])
              : ((block_size >> sub_log2[0]) - lag));
      for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
          const double residual =
              noise_model->params.use_highbd
                  ? eval_ar_residual_highbd(
                        noise_model->coords, coeffs, num_coords,
                        (const uint16_t *const)data,
                        (const uint16_t *const)denoised, stride, sub_log2,
                        (const uint16_t *const)alt_data,
                        (const uint16_t *const)alt_denoised, alt_stride,
                        x + x_o, y + y_o)
                  : eval_ar_residual_lowbd(noise_model->coords, coeffs,
                                           num_coords, data, denoised, stride,
                                           sub_log2, alt_data, alt_denoised,
                                           alt_stride, x + x_o, y + y_o);
          noise_var += residual * residual;
          num_samples_in_block++;
        }
      }
      if (num_samples_in_block > block_size) {
        const double noise_std = sqrt(noise_var / num_samples_in_block);
        aom_noise_strength_solver_add_measurement(noise_strength_solver,
                                                  block_mean, noise_std);
      }
    }
  }
}

// Return true if the noise estimate appears to be different from the combined
// (multi-frame) estimate. The difference is measured by checking whether the
// AR coefficients have diverged (using a threshold on normalized cross
// correlation), or whether the noise strength has changed.
static int is_noise_model_different(aom_noise_model_t *const noise_model) {
  // These thresholds are kind of arbitrary and will likely need further tuning
  // (or exported as parameters). The threshold on noise strength is a weighted
  // difference between the noise strength histograms
  const double kCoeffThreshold = 0.9;
  const double kStrengthThreshold =
      0.005 * (1 << (noise_model->params.bit_depth - 8));
  for (int c = 0; c < 1; ++c) {
    const double corr =
        aom_normalized_cross_correlation(noise_model->latest_state[c].eqns.x,
                                         noise_model->combined_state[c].eqns.x,
                                         noise_model->combined_state[c].eqns.n);
    if (corr < kCoeffThreshold) return 1;

    const double dx =
        1.0 / noise_model->latest_state[c].strength_solver.num_bins;

    const aom_equation_system_t *latest_eqns =
        &noise_model->latest_state[c].strength_solver.eqns;
    const aom_equation_system_t *combined_eqns =
        &noise_model->combined_state[c].strength_solver.eqns;
    double diff = 0;
    double total_weight = 0;
    for (int j = 0; j < latest_eqns->n; ++j) {
      double weight = 0;
      for (int i = 0; i < latest_eqns->n; ++i) {
        weight += latest_eqns->A[i * latest_eqns->n + j];
      }
      weight = sqrt(weight);
      diff += weight * fabs(latest_eqns->x[j] - combined_eqns->x[j]);
      total_weight += weight;
    }
    if (diff * dx / total_weight > kStrengthThreshold) return 1;
  }
  return 0;
}

aom_noise_status_t aom_noise_model_update(
    aom_noise_model_t *const noise_model, const uint8_t *const data[3],
    const uint8_t *const denoised[3], int w, int h, int stride[3],
    int chroma_sub_log2[2], const uint8_t *const flat_blocks, int block_size) {
  const int num_blocks_w = (w + block_size - 1) / block_size;
  const int num_blocks_h = (h + block_size - 1) / block_size;
  int y_model_different = 0;
  int num_blocks = 0;
  int i = 0, channel = 0;

  if (block_size <= 1) {
    fprintf(stderr, "block_size = %d must be > 1\n", block_size);
    return AOM_NOISE_STATUS_INVALID_ARGUMENT;
  }

  if (block_size < noise_model->params.lag * 2 + 1) {
    fprintf(stderr, "block_size = %d must be >= %d\n", block_size,
            noise_model->params.lag * 2 + 1);
    return AOM_NOISE_STATUS_INVALID_ARGUMENT;
  }

  // Clear the latest equation system
  for (i = 0; i < 3; ++i) {
    equation_system_clear(&noise_model->latest_state[i].eqns);
    noise_strength_solver_clear(&noise_model->latest_state[i].strength_solver);
  }

  // Check that we have enough flat blocks
  for (i = 0; i < num_blocks_h * num_blocks_w; ++i) {
    if (flat_blocks[i]) {
      num_blocks++;
    }
  }

  if (num_blocks <= 1) {
    fprintf(stderr, "Not enough flat blocks to update noise estimate\n");
    return AOM_NOISE_STATUS_INSUFFICIENT_FLAT_BLOCKS;
  }

  for (channel = 0; channel < 3; ++channel) {
    int no_subsampling[2] = { 0, 0 };
    const uint8_t *alt_data = channel > 0 ? data[0] : 0;
    const uint8_t *alt_denoised = channel > 0 ? denoised[0] : 0;
    int *sub = channel > 0 ? chroma_sub_log2 : no_subsampling;
    if (!data[channel] || !denoised[channel]) break;
    if (!add_block_observations(noise_model, channel, data[channel],
                                denoised[channel], w, h, stride[channel], sub,
                                alt_data, alt_denoised, stride[0], flat_blocks,
                                block_size, num_blocks_w, num_blocks_h)) {
      fprintf(stderr, "Adding block observation failed\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    if (!equation_system_solve(&noise_model->latest_state[channel].eqns)) {
      if (channel > 0) {
        set_chroma_coefficient_fallback_soln(
            &noise_model->latest_state[channel].eqns);
      } else {
        fprintf(stderr, "Solving latest noise equation system failed %d!\n",
                channel);
        return AOM_NOISE_STATUS_INTERNAL_ERROR;
      }
    }

    add_noise_std_observations(
        noise_model, channel, noise_model->latest_state[channel].eqns.x,
        data[channel], denoised[channel], w, h, stride[channel], sub, alt_data,
        alt_denoised, stride[0], flat_blocks, block_size, num_blocks_w,
        num_blocks_h);

    if (!aom_noise_strength_solver_solve(
            &noise_model->latest_state[channel].strength_solver)) {
      fprintf(stderr, "Solving latest noise strength failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    // Check noise characteristics and return if error.
    if (channel == 0 &&
        noise_model->combined_state[channel].strength_solver.num_equations >
            0 &&
        is_noise_model_different(noise_model)) {
      y_model_different = 1;
    }

    // Don't update the combined stats if the y model is different.
    if (y_model_different) continue;

    equation_system_add(&noise_model->combined_state[channel].eqns,
                        &noise_model->latest_state[channel].eqns);
    if (!equation_system_solve(&noise_model->combined_state[channel].eqns)) {
      if (channel > 0) {
        set_chroma_coefficient_fallback_soln(
            &noise_model->combined_state[channel].eqns);
      } else {
        fprintf(stderr, "Solving combined noise equation system failed %d!\n",
                channel);
        return AOM_NOISE_STATUS_INTERNAL_ERROR;
      }
    }

    noise_strength_solver_add(
        &noise_model->combined_state[channel].strength_solver,
        &noise_model->latest_state[channel].strength_solver);

    if (!aom_noise_strength_solver_solve(
            &noise_model->combined_state[channel].strength_solver)) {
      fprintf(stderr, "Solving combined noise strength failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }
  }

  return y_model_different ? AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE
                           : AOM_NOISE_STATUS_OK;
}

void aom_noise_model_save_latest(aom_noise_model_t *noise_model) {
  for (int c = 0; c < 3; c++) {
    equation_system_copy(&noise_model->combined_state[c].eqns,
                         &noise_model->latest_state[c].eqns);
    equation_system_copy(&noise_model->combined_state[c].strength_solver.eqns,
                         &noise_model->latest_state[c].strength_solver.eqns);
    noise_model->combined_state[c].strength_solver.num_equations =
        noise_model->latest_state[c].strength_solver.num_equations;
    noise_model->combined_state[c].strength_solver.total =
        noise_model->latest_state[c].strength_solver.total;
  }
}

int aom_noise_model_get_grain_parameters(aom_noise_model_t *const noise_model,
                                         aom_film_grain_t *film_grain) {
  if (noise_model->params.lag > 3) {
    fprintf(stderr, "params.lag = %d > 3\n", noise_model->params.lag);
    return 0;
  }
  memset(film_grain, 0, sizeof(*film_grain));

  film_grain->apply_grain = 1;
  film_grain->update_parameters = 1;

  film_grain->ar_coeff_lag = noise_model->params.lag;

  // Convert the scaling functions to 8 bit values
  aom_noise_strength_lut_t scaling_points[3];
  aom_noise_strength_solver_fit_piecewise(
      &noise_model->combined_state[0].strength_solver, 14, scaling_points + 0);
  aom_noise_strength_solver_fit_piecewise(
      &noise_model->combined_state[1].strength_solver, 10, scaling_points + 1);
  aom_noise_strength_solver_fit_piecewise(
      &noise_model->combined_state[2].strength_solver, 10, scaling_points + 2);

  // Both the domain and the range of the scaling functions in the film_grain
  // are normalized to 8-bit (e.g., they are implicitly scaled during grain
  // synthesis).
  const double strength_divisor = 1 << (noise_model->params.bit_depth - 8);
  double max_scaling_value = 1e-4;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < scaling_points[c].num_points; ++i) {
      scaling_points[c].points[i][0] =
          AOMMIN(255, scaling_points[c].points[i][0] / strength_divisor);
      scaling_points[c].points[i][1] =
          AOMMIN(255, scaling_points[c].points[i][1] / strength_divisor);
      max_scaling_value =
          AOMMAX(scaling_points[c].points[i][1], max_scaling_value);
    }
  }

  // Scaling_shift values are in the range [8,11]
  const int max_scaling_value_log2 =
      clamp((int)floor(log2(max_scaling_value) + 1), 2, 5);
  film_grain->scaling_shift = 5 + (8 - max_scaling_value_log2);

  const double scale_factor = 1 << (8 - max_scaling_value_log2);
  film_grain->num_y_points = scaling_points[0].num_points;
  film_grain->num_cb_points = scaling_points[1].num_points;
  film_grain->num_cr_points = scaling_points[2].num_points;

  int(*film_grain_scaling[3])[2] = {
    film_grain->scaling_points_y,
    film_grain->scaling_points_cb,
    film_grain->scaling_points_cr,
  };
  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < scaling_points[c].num_points; ++i) {
      film_grain_scaling[c][i][0] = (int)(scaling_points[c].points[i][0] + 0.5);
      film_grain_scaling[c][i][1] = clamp(
          (int)(scale_factor * scaling_points[c].points[i][1] + 0.5), 0, 255);
    }
  }
  aom_noise_strength_lut_free(scaling_points + 0);
  aom_noise_strength_lut_free(scaling_points + 1);
  aom_noise_strength_lut_free(scaling_points + 2);

  // Convert the ar_coeffs into 8-bit values
  double max_coeff = 1e-4, min_coeff = -1e-4;
  for (int c = 0; c < 3; c++) {
    aom_equation_system_t *eqns = &noise_model->combined_state[c].eqns;
    for (int i = 0; i < eqns->n; ++i) {
      max_coeff = AOMMAX(max_coeff, eqns->x[i]);
      min_coeff = AOMMIN(min_coeff, eqns->x[i]);
    }
  }
  // Shift value: AR coeffs range (values 6-9)
  // 6: [-2, 2),  7: [-1, 1), 8: [-0.5, 0.5), 9: [-0.25, 0.25)
  film_grain->ar_coeff_shift =
      clamp(7 - (int)AOMMAX(1 + floor(log2(max_coeff)), ceil(log2(-min_coeff))),
            6, 9);
  double scale_ar_coeff = 1 << film_grain->ar_coeff_shift;
  int *ar_coeffs[3] = {
    film_grain->ar_coeffs_y,
    film_grain->ar_coeffs_cb,
    film_grain->ar_coeffs_cr,
  };
  for (int c = 0; c < 3; ++c) {
    aom_equation_system_t *eqns = &noise_model->combined_state[c].eqns;
    for (int i = 0; i < eqns->n; ++i) {
      ar_coeffs[c][i] =
          clamp((int)round(scale_ar_coeff * eqns->x[i]), -128, 127);
    }
  }

  // At the moment, the noise modeling code assumes that the chroma scaling
  // functions are a function of luma.
  film_grain->cb_mult = 128;       // 8 bits
  film_grain->cb_luma_mult = 192;  // 8 bits
  film_grain->cb_offset = 256;     // 9 bits

  film_grain->cr_mult = 128;       // 8 bits
  film_grain->cr_luma_mult = 192;  // 8 bits
  film_grain->cr_offset = 256;     // 9 bits

  film_grain->chroma_scaling_from_luma = 0;
  film_grain->grain_scale_shift = 0;
  return 1;
}
