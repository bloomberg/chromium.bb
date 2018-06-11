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

#include "aom_dsp/noise_util.h"
#include "aom_mem/aom_mem.h"

double aom_normalized_cross_correlation(const double *a, const double *b,
                                        int n) {
  double c = 0;
  double a_len = 0;
  double b_len = 0;
  for (int i = 0; i < n; ++i) {
    a_len += a[i] * a[i];
    b_len += b[i] * b[i];
    c += a[i] * b[i];
  }
  return c / (sqrt(a_len) * sqrt(b_len));
}

int aom_noise_data_validate(const double *data, int w, int h) {
  const double kVarianceThreshold = 2;
  const double kMeanThreshold = 2;

  int x = 0, y = 0;
  int ret_value = 1;
  double var = 0, mean = 0;
  double *mean_x, *mean_y, *var_x, *var_y;

  // Check that noise variance is not increasing in x or y
  // and that the data is zero mean.
  mean_x = (double *)aom_malloc(sizeof(*mean_x) * w);
  var_x = (double *)aom_malloc(sizeof(*var_x) * w);
  mean_y = (double *)aom_malloc(sizeof(*mean_x) * h);
  var_y = (double *)aom_malloc(sizeof(*var_y) * h);

  memset(mean_x, 0, sizeof(*mean_x) * w);
  memset(var_x, 0, sizeof(*var_x) * w);
  memset(mean_y, 0, sizeof(*mean_y) * h);
  memset(var_y, 0, sizeof(*var_y) * h);

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const double d = data[y * w + x];
      var_x[x] += d * d;
      var_y[y] += d * d;
      mean_x[x] += d;
      mean_y[y] += d;
      var += d * d;
      mean += d;
    }
  }
  mean /= (w * h);
  var = var / (w * h) - mean * mean;

  for (y = 0; y < h; ++y) {
    mean_y[y] /= h;
    var_y[y] = var_y[y] / h - mean_y[y] * mean_y[y];
    if (fabs(var_y[y] - var) >= kVarianceThreshold) {
      fprintf(stderr, "Variance distance too large %f %f\n", var_y[y], var);
      ret_value = 0;
      break;
    }
    if (fabs(mean_y[y] - mean) >= kMeanThreshold) {
      fprintf(stderr, "Mean distance too large %f %f\n", mean_y[y], mean);
      ret_value = 0;
      break;
    }
  }

  for (x = 0; x < w; ++x) {
    mean_x[x] /= w;
    var_x[x] = var_x[x] / w - mean_x[x] * mean_x[x];
    if (fabs(var_x[x] - var) >= kVarianceThreshold) {
      fprintf(stderr, "Variance distance too large %f %f\n", var_x[x], var);
      ret_value = 0;
      break;
    }
    if (fabs(mean_x[x] - mean) >= kMeanThreshold) {
      fprintf(stderr, "Mean distance too large %f %f\n", mean_x[x], mean);
      ret_value = 0;
      break;
    }
  }

  aom_free(mean_x);
  aom_free(mean_y);
  aom_free(var_x);
  aom_free(var_y);

  return ret_value;
}
