// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_COMPARATOR_H_
#define CC_TEST_PIXEL_COMPARATOR_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// Interface for pixel comparators.
class PixelComparator {
 public:
  virtual bool Compare(const SkBitmap& actual_bmp,
                       const SkBitmap& expected_bmp) const = 0;

 protected:
  virtual ~PixelComparator() {}
};

// Exact pixel comparator. Counts the number of pixel with an error.
class ExactPixelComparator : public PixelComparator {
 public:
  explicit ExactPixelComparator(const bool discard_alpha);
  // Returns true if the two bitmaps are identical. Otherwise, returns false
  // and report the number of pixels with an error on LOG(ERROR). Differences
  // in the alpha channel are ignored.
  virtual bool Compare(const SkBitmap& actual_bmp,
                       const SkBitmap& expected_bmp) const OVERRIDE;

 private:
  // Exclude alpha channel from comparison?
  bool discard_alpha_;
};

// Fuzzy pixel comparator. Counts small and arbitrary errors separately and
// computes average and maximum absolute errors per color channel.
class FuzzyPixelComparator : public PixelComparator {
 public:
  FuzzyPixelComparator(const bool discard_alpha,
                       const float error_pixels_percentage_limit,
                       const float small_error_pixels_percentage_limit,
                       const float avg_abs_error_limit,
                       const int max_abs_error_limit,
                       const int small_error_threshold);
  // Computes error metrics and returns true if the errors don't exceed the
  // specified limits. Otherwise, returns false and reports the error metrics on
  // LOG(ERROR). Differences in the alpha channel are ignored.
  virtual bool Compare(const SkBitmap& actual_bmp,
                       const SkBitmap& expected_bmp) const OVERRIDE;

 private:
  // Exclude alpha channel from comparison?
  bool discard_alpha_;
  // Limit for percentage of pixels with an error.
  float error_pixels_percentage_limit_;
  // Limit for percentage of pixels with a small error.
  float small_error_pixels_percentage_limit_;
  // Limit for average absolute error (excluding identical pixels).
  float avg_abs_error_limit_;
  // Limit for largest absolute error.
  int max_abs_error_limit_;
  // Threshold for small errors.
  int small_error_threshold_;
};

}  // namespace cc

#endif  // CC_TEST_PIXEL_COMPARATOR_H_
