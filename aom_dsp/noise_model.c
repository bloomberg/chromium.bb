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
static const double kBlockNormalization = 255.0;
static const int kMaxLag = 4;

static double get_block_mean(const uint8_t *data, int w, int h, int stride,
                             int x_o, int y_o, int block_size) {
  const int max_h = AOMMIN(h - y_o, block_size);
  const int max_w = AOMMIN(w - x_o, block_size);
  double block_mean = 0;
  for (int y = 0; y < max_h; ++y) {
    for (int x = 0; x < max_w; ++x) {
      block_mean += data[(y_o + y) * stride + x_o + x];
    }
  }
  return block_mean / (max_w * max_h);
}

static void equation_system_clear(aom_equation_system_t *eqns) {
  const int n = eqns->n;
  memset(eqns->A, 0, sizeof(*eqns->A) * n * n);
  memset(eqns->x, 0, sizeof(*eqns->x) * n);
  memset(eqns->b, 0, sizeof(*eqns->b) * n);
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

static int noise_state_init(aom_noise_state_t *state, int n) {
  const int kNumBins = 20;
  if (!equation_system_init(&state->eqns, n)) {
    fprintf(stderr, "Failed initialization noise state with size %d\n", n);
    return 0;
  }
  return aom_noise_strength_solver_init(&state->strength_solver, kNumBins);
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

void aom_noise_strength_solver_add_measurement(
    aom_noise_strength_solver_t *solver, double block_mean, double noise_std) {
  const double val =
      AOMMIN(AOMMAX(block_mean, solver->min_intensity), solver->max_intensity);
  const double range = solver->max_intensity - solver->min_intensity;
  const double bin =
      (solver->num_bins - 1) * (val - solver->min_intensity) / range;
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
                                   int num_bins) {
  if (!solver) return 0;
  memset(solver, 0, sizeof(*solver));
  solver->num_bins = num_bins;
  solver->min_intensity = 0;
  solver->max_intensity = 255;
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

int aom_noise_strength_solver_fit_piecewise(
    const aom_noise_strength_solver_t *solver, aom_noise_strength_lut_t *lut) {
  const double kTolerance = 0.1;
  int low_point = 0;
  aom_equation_system_t sys;
  if (!equation_system_init(&sys, 2)) {
    fprintf(stderr, "Failed to init equation system\n");
    return 0;
  }

  if (!aom_noise_strength_lut_init(lut, solver->num_bins + 1)) {
    fprintf(stderr, "Failed to init lut\n");
    return 0;
  }

  lut->points[0][0] = aom_noise_strength_solver_get_center(solver, 0);
  lut->points[0][1] = solver->eqns.x[0];
  lut->num_points = 1;

  while (low_point < solver->num_bins - 1) {
    int i = low_point;
    equation_system_clear(&sys);
    for (; i < solver->num_bins; ++i) {
      int x = i - low_point;
      double b = 1;
      sys.A[0 * 2 + 0] += x * x;
      sys.A[0 * 2 + 1] += x * b;
      sys.A[1 * 2 + 0] += x * b;
      sys.A[1 * 2 + 1] += b * b;
      sys.b[0] += x * solver->eqns.x[i];
      sys.b[1] += b * solver->eqns.x[i];

      if (x > 1) {
        double res = 0;
        int k;
        equation_system_solve(&sys);

        for (k = low_point; k <= i; ++k) {
          double y;
          x = k - low_point;
          y = sys.x[0] * x + sys.x[1];
          y -= solver->eqns.x[k];
          res += y * y;
        }
        const int n = i - low_point + 1;
        if (sqrt(res / n) > kTolerance) {
          low_point = i - 1;

          lut->points[lut->num_points][0] =
              aom_noise_strength_solver_get_center(solver, i - 1);
          lut->points[lut->num_points][1] = solver->eqns.x[i - 1];
          lut->num_points++;
        }
      }
    }
    if (i == solver->num_bins) {
      lut->points[lut->num_points][0] =
          aom_noise_strength_solver_get_center(solver, i - 1);
      lut->points[lut->num_points][1] = solver->eqns.x[i - 1];
      lut->num_points++;
      break;
    }
  }
  equation_system_free(&sys);
  return 1;
}

int aom_flat_block_finder_init(aom_flat_block_finder_t *block_finder,
                               int block_size) {
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

  for (yi = 0; yi < block_size; ++yi) {
    const int y = AOMMAX(0, AOMMIN(h - 1, offsy + yi));
    for (xi = 0; xi < block_size; ++xi) {
      const int x = AOMMAX(0, AOMMIN(w - 1, offsx + xi));
      block[yi * block_size + xi] =
          ((double)data[y * stride + x]) / kBlockNormalization;
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
    if (!noise_state_init(&model->combined_state[c], n + (c > 0))) {
      fprintf(stderr, "Failed to allocate noise state for channel %d\n", c);
      aom_noise_model_free(model);
      return 0;
    }
    if (!noise_state_init(&model->latest_state[c], n + (c > 0))) {
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

static int add_block_observations(
    aom_noise_model_t *noise_model, int c, const uint8_t *const data,
    const uint8_t *const denoised, int w, int h, int stride, int sub_log2[2],
    const uint8_t *const alt_data, const uint8_t *const alt_denoised,
    int alt_stride, const uint8_t *const flat_blocks, int block_size,
    int num_blocks_w, int num_blocks_h) {
  const int lag = noise_model->params.lag;
  const int num_coords = noise_model->n;
  double *A = noise_model->latest_state[c].eqns.A;
  double *b = noise_model->latest_state[c].eqns.b;
  double *buffer = (double *)aom_malloc(sizeof(*buffer) * (num_coords + 1));
  const int n = noise_model->latest_state[c].eqns.n;
  int bx, by;
  (void)sub_log2;

  if (!buffer) {
    fprintf(stderr, "Unable to allocate buffer of size %d\n", num_coords + 1);
    return 0;
  }
  for (by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * block_size;
    for (bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * block_size;
      int x_start = 0, y_start = 0, x_end = 0, y_end = 0;
      int x, y, i, j;
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      y_start = (by > 0 && flat_blocks[(by - 1) * num_blocks_w + bx]) ? 0 : lag;
      x_start = (bx > 0 && flat_blocks[by * num_blocks_w + bx - 1]) ? 0 : lag;
      y_end = AOMMIN(
          h - by * block_size,
          (by + 1 < num_blocks_h && flat_blocks[(by + 1) * num_blocks_w + bx])
              ? block_size
              : block_size - lag);
      x_end = AOMMIN(
          w - bx * block_size - lag,
          (bx + 1 < num_blocks_w && flat_blocks[by * num_blocks_w + bx + 1])
              ? block_size
              : block_size - lag);
      for (y = y_start; y < y_end; ++y) {
        for (x = x_start; x < x_end; ++x) {
          double val = 0;
          for (i = 0; i < num_coords; ++i) {
            const int dx_i = noise_model->coords[i][0];
            const int dy_i = noise_model->coords[i][1];
            const int x_i = x_o + x + dx_i;
            const int y_i = y_o + y + dy_i;
            assert(x_i < w && y_i < h);
            buffer[i] = ((double)(data[y_i * stride + x_i]) -
                         (double)(denoised[y_i * stride + x_i]));
          }
          val = ((double)data[(y_o + y) * stride + (x_o + x)]) -
                ((double)denoised[(y_o + y) * stride + (x_o + x)]);

          // For the color channels we must also consider the correlation with
          // the luma channel.
          if (alt_data && alt_denoised) {
            buffer[num_coords] =
                ((double)alt_data[(y_o + y) * alt_stride + (x_o + x)]) -
                ((double)alt_denoised[(y_o + y) * alt_stride + (x_o + x)]);
          }

          for (i = 0; i < n; ++i) {
            for (j = 0; j < n; ++j) {
              A[i * n + j] += (buffer[i] * buffer[j]) /
                              (kBlockNormalization * kBlockNormalization);
            }
            b[i] +=
                (buffer[i] * val) / (kBlockNormalization * kBlockNormalization);
          }
        }
      }
    }
  }
  aom_free(buffer);
  return 1;
}

void add_noise_std_observations(aom_noise_model_t *noise_model, int c,
                                const double *coeffs, const uint8_t *const data,
                                const uint8_t *const denoised, int w, int h,
                                int stride, const uint8_t *const alt_data,
                                const uint8_t *const alt_denoised,
                                int alt_stride,
                                const uint8_t *const flat_blocks,
                                int block_size, int num_blocks_w,
                                int num_blocks_h) {
  const int lag = noise_model->params.lag;
  const int num_coords = noise_model->n;
  aom_noise_strength_solver_t *noise_strength_solver =
      &noise_model->latest_state[c].strength_solver;
  int bx = 0, by = 0;

  for (by = 0; by < num_blocks_h; ++by) {
    const int y_o = by * block_size;
    for (bx = 0; bx < num_blocks_w; ++bx) {
      const int x_o = bx * block_size;
      if (!flat_blocks[by * num_blocks_w + bx]) {
        continue;
      }
      const double block_mean =
          get_block_mean(alt_data ? alt_data : data, w, h,
                         alt_data ? alt_stride : stride, x_o, y_o, block_size);
      double noise_var = 0;
      int num_samples_in_block = 0;
      int y_start =
          (by > 0 && flat_blocks[(by - 1) * num_blocks_w + bx]) ? 0 : lag;
      int x_start =
          (bx > 0 && flat_blocks[by * num_blocks_w + bx - 1]) ? 0 : lag;
      int y_end =
          (by + 1 < num_blocks_h && flat_blocks[(by + 1) * num_blocks_w + bx])
              ? block_size
              : block_size - lag;
      int x_end =
          (bx + 1 < num_blocks_w && flat_blocks[by * num_blocks_w + bx + 1])
              ? block_size
              : block_size - lag;
      for (int y = y_start; y < y_end; ++y) {
        for (int x = x_start; x < x_end; ++x) {
          const double actual =
              ((double)(data[(y_o + y) * stride + (x_o + x)]) -
               (double)(denoised[(y_o + y) * stride + (x_o + x)]));
          double sum = 0;
          for (int i = 0; i < num_coords; ++i) {
            const int dx_i = noise_model->coords[i][0];
            const int dy_i = noise_model->coords[i][1];
            const int x_i = x_o + x + dx_i;
            const int y_i = y_o + y + dy_i;
            sum += coeffs[i] * ((double)(data[y_i * stride + x_i]) -
                                (double)(denoised[y_i * stride + x_i]));
          }
          if (alt_data && alt_denoised) {
            sum += coeffs[num_coords] *
                   ((double)(alt_data[(y_o + y) * stride + (x_o + x)]) -
                    (double)(alt_denoised[(y_o + y) * stride + (x_o + x)]));
          }
          noise_var += (sum - actual) * (sum - actual);
          num_samples_in_block++;
        }
      }
      const double noise_std = sqrt(noise_var / num_samples_in_block);
      aom_noise_strength_solver_add_measurement(noise_strength_solver,
                                                block_mean, noise_std);
    }
  }
}

aom_noise_status_t aom_noise_model_update(
    aom_noise_model_t *const noise_model, const uint8_t *const data[3],
    const uint8_t *const denoised[3], int w, int h, int stride[3],
    int chroma_sub[2], const uint8_t *const flat_blocks, int block_size) {
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
    const uint8_t *alt_data = channel > 0 ? data[0] : 0;
    const uint8_t *alt_denoised = channel > 0 ? denoised[0] : 0;
    int *sub = channel > 0 ? chroma_sub : 0;
    if (!data[channel] || !denoised[channel]) break;

    if (!add_block_observations(noise_model, channel, data[channel],
                                denoised[channel], w, h, stride[channel], sub,
                                alt_data, alt_denoised, stride[0], flat_blocks,
                                block_size, num_blocks_w, num_blocks_h)) {
      fprintf(stderr, "Adding block observation failed\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    if (!equation_system_solve(&noise_model->latest_state[channel].eqns)) {
      fprintf(stderr, "Solving latest noise equation system failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
    }

    add_noise_std_observations(
        noise_model, channel, noise_model->latest_state[channel].eqns.x,
        data[channel], denoised[channel], w, h, stride[channel], alt_data,
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
            0) {
      y_model_different = 1;
    }

    // Don't update the combined stats if the y model is different.
    if (y_model_different) continue;

    equation_system_add(&noise_model->combined_state[channel].eqns,
                        &noise_model->latest_state[channel].eqns);
    if (!equation_system_solve(&noise_model->combined_state[channel].eqns)) {
      fprintf(stderr, "Solving combined noise equation failed!\n");
      return AOM_NOISE_STATUS_INTERNAL_ERROR;
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
