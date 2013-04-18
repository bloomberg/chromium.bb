// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/content_analysis.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <numeric>
#include <vector>

#include "base/logging.h"
#include "skia/ext/convolver.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/color_analysis.h"

namespace {

template<class InputIterator, class OutputIterator, class Compare>
void SlidingWindowMinMax(InputIterator first,
                         InputIterator last,
                         OutputIterator output,
                         int window_size,
                         Compare cmp) {
  typedef std::deque<
      std::pair<typename std::iterator_traits<InputIterator>::value_type, int> >
          deque_type;
  deque_type slider;
  int front_tail_length = window_size / 2;
  int i = 0;
  DCHECK_LT(front_tail_length, last - first);
  // This min-max filter functions the way image filters do. The min/max we
  // compute is placed in the center of the window. Thus, first we need to
  // 'pre-load' the window with the slider with right-tail of the filter.
  for (; first < last && i < front_tail_length; ++i, ++first)
    slider.push_back(std::make_pair(*first, i));

  for (; first < last; ++i, ++first, ++output) {
    while (!slider.empty() && !cmp(slider.back().first, *first))
      slider.pop_back();
    slider.push_back(std::make_pair(*first, i));

    while (slider.front().second <= i - window_size)
      slider.pop_front();
    *output = slider.front().first;
  }

  // Now at the tail-end we will simply need to use whatever value is left of
  // the filter to compute the remaining front_tail_length taps in the output.

  // If input shorter than window, remainder length needs to be adjusted.
  front_tail_length = std::min(front_tail_length, i);
  for (; front_tail_length >= 0; --front_tail_length, ++i) {
    while (slider.front().second <= i - window_size)
      slider.pop_front();
    *output = slider.front().first;
  }
}

}  // namespace

namespace thumbnailing_utils {

void ApplyGaussianGradientMagnitudeFilter(SkBitmap* input_bitmap,
                                          float kernel_sigma) {
  // The purpose of this function is to highlight salient
  // (attention-attracting?) features of the image for use in image
  // retargetting.
  SkAutoLockPixels source_lock(*input_bitmap);
  DCHECK(input_bitmap);
  DCHECK(input_bitmap->getPixels());
  DCHECK_EQ(SkBitmap::kA8_Config, input_bitmap->config());

  const int tail_length = static_cast<int>(4.0f * kernel_sigma + 0.5f);
  const int kernel_size = tail_length * 2 + 1;
  const float sigmasq = kernel_sigma * kernel_sigma;
  std::vector<float> smoother_weights(kernel_size, 0.0);
  float kernel_sum = 1.0f;

  smoother_weights[tail_length] = 1.0f;

  for (int ii = 1; ii <= tail_length; ++ii) {
    float v = std::exp(-0.5f * ii * ii / sigmasq);
    smoother_weights[tail_length + ii] = v;
    smoother_weights[tail_length - ii] = v;
    kernel_sum += 2.0f * v;
  }

  for (int i = 0; i < kernel_size; ++i)
    smoother_weights[i] /= kernel_sum;

  std::vector<float> gradient_weights(kernel_size, 0.0);
  gradient_weights[tail_length] = 0.0;
  for (int ii = 1; ii <= tail_length; ++ii) {
    float v = sigmasq * smoother_weights[tail_length + ii] / ii;
    gradient_weights[tail_length + ii] = v;
    gradient_weights[tail_length - ii] = -v;
  }

  skia::ConvolutionFilter1D smoothing_filter;
  skia::ConvolutionFilter1D gradient_filter;
  smoothing_filter.AddFilter(0, &smoother_weights[0], smoother_weights.size());
  gradient_filter.AddFilter(0, &gradient_weights[0], gradient_weights.size());

  // To perform computations we will need one intermediate buffer. It can
  // very well be just another bitmap.
  const SkISize image_size = SkISize::Make(input_bitmap->width(),
                                           input_bitmap->height());
  SkBitmap intermediate;
  intermediate.setConfig(
      input_bitmap->config(), image_size.width(), image_size.height());
  intermediate.allocPixels();

  skia::SingleChannelConvolveX1D(
      input_bitmap->getAddr8(0, 0),
      static_cast<int>(input_bitmap->rowBytes()),
      0, input_bitmap->bytesPerPixel(),
      smoothing_filter,
      image_size,
      intermediate.getAddr8(0, 0),
      static_cast<int>(intermediate.rowBytes()),
      0, intermediate.bytesPerPixel(), false);
  skia::SingleChannelConvolveY1D(
      intermediate.getAddr8(0, 0),
      static_cast<int>(intermediate.rowBytes()),
      0, intermediate.bytesPerPixel(),
      smoothing_filter,
      image_size,
      input_bitmap->getAddr8(0, 0),
      static_cast<int>(input_bitmap->rowBytes()),
      0, input_bitmap->bytesPerPixel(), false);

  // Now the gradient operator (we will need two buffers).
  SkBitmap intermediate2;
  intermediate2.setConfig(
      input_bitmap->config(), image_size.width(), image_size.height());
  intermediate2.allocPixels();

  skia::SingleChannelConvolveX1D(
      input_bitmap->getAddr8(0, 0),
      static_cast<int>(input_bitmap->rowBytes()),
      0, input_bitmap->bytesPerPixel(),
      gradient_filter,
      image_size,
      intermediate.getAddr8(0, 0),
      static_cast<int>(intermediate.rowBytes()),
      0, intermediate.bytesPerPixel(), true);
  skia::SingleChannelConvolveY1D(
      input_bitmap->getAddr8(0, 0),
      static_cast<int>(input_bitmap->rowBytes()),
      0, input_bitmap->bytesPerPixel(),
      gradient_filter,
      image_size,
      intermediate2.getAddr8(0, 0),
      static_cast<int>(intermediate2.rowBytes()),
      0, intermediate2.bytesPerPixel(), true);

  unsigned grad_max = 0;
  for (int r = 0; r < image_size.height(); ++r) {
    const uint8* grad_x_row = intermediate.getAddr8(0, r);
    const uint8* grad_y_row = intermediate2.getAddr8(0, r);
    for (int c = 0; c < image_size.width(); ++c) {
      unsigned grad_x = grad_x_row[c];
      unsigned grad_y = grad_y_row[c];
      grad_max = std::max(grad_max, grad_x * grad_x + grad_y * grad_y);
    }
  }

  int bit_shift = 0;
  if (grad_max > 255)
    bit_shift = static_cast<int>(
        std::log10(static_cast<float>(grad_max)) / std::log10(2.0f)) - 7;
  for (int r = 0; r < image_size.height(); ++r) {
    const uint8* grad_x_row =intermediate.getAddr8(0, r);
    const uint8* grad_y_row = intermediate2.getAddr8(0, r);
    uint8* target_row = input_bitmap->getAddr8(0, r);
    for (int c = 0; c < image_size.width(); ++c) {
      unsigned grad_x = grad_x_row[c];
      unsigned grad_y = grad_y_row[c];
      target_row[c] = (grad_x * grad_x + grad_y * grad_y) >> bit_shift;
    }
  }
}

void ExtractImageProfileInformation(const SkBitmap& input_bitmap,
                                    const gfx::Rect& area,
                                    const gfx::Size& target_size,
                                    bool apply_log,
                                    std::vector<float>* rows,
                                    std::vector<float>* columns) {
  SkAutoLockPixels source_lock(input_bitmap);
  DCHECK(rows);
  DCHECK(columns);
  DCHECK(input_bitmap.getPixels());
  DCHECK_EQ(SkBitmap::kA8_Config, input_bitmap.config());
  DCHECK_GE(area.x(), 0);
  DCHECK_GE(area.y(), 0);
  DCHECK_LE(area.right(), input_bitmap.width());
  DCHECK_LE(area.bottom(), input_bitmap.height());

  // Make sure rows and columns are allocated and initialized to 0.
  rows->clear();
  columns->clear();
  rows->resize(area.height(), 0);
  columns->resize(area.width(), 0);

  for (int r = 0; r < area.height(); ++r) {
    // Points to the first byte of the row in the rectangle.
    const uint8* image_row = input_bitmap.getAddr8(area.x(), r + area.y());
    unsigned row_sum = 0;
    for (int c = 0; c < area.width(); ++c, ++image_row) {
      row_sum += *image_row;
      (*columns)[c] += *image_row;
    }
    (*rows)[r] = row_sum;
  }

  if (apply_log) {
    // Generally for processing we will need to take logarithm of this data.
    // The option not to apply it is left principally as a test seam.
    std::vector<float>::iterator it;
    for (it = columns->begin(); it < columns->end(); ++it)
      *it = std::log(1.0f + *it);

    for (it = rows->begin(); it < rows->end(); ++it)
      *it = std::log(1.0f + *it);
  }

  if (!target_size.IsEmpty()) {
    // If the target size is given, profiles should be further processed through
    // morphological closing. The idea is to close valleys smaller than what
    // can be seen after scaling down to avoid deforming noticable features
    // when profiles are used.
    // Morphological closing is defined as dilation followed by errosion. In
    // normal-speak: sliding-window maximum followed by minimum.
    int column_window_size = 1 + 2 *
        static_cast<int>(0.5f * area.width() / target_size.width() + 0.5f);
    int row_window_size = 1 + 2 *
        static_cast<int>(0.5f * area.height() / target_size.height() + 0.5f);

    // Dilate and erode each profile with the given window size.
    if (column_window_size >= 3) {
      SlidingWindowMinMax(columns->begin(),
                          columns->end(),
                          columns->begin(),
                          column_window_size,
                          std::greater<float>());
      SlidingWindowMinMax(columns->begin(),
                          columns->end(),
                          columns->begin(),
                          column_window_size,
                          std::less<float>());
    }

    if (row_window_size >= 3) {
      SlidingWindowMinMax(rows->begin(),
                          rows->end(),
                          rows->begin(),
                          row_window_size,
                          std::greater<float>());
      SlidingWindowMinMax(rows->begin(),
                          rows->end(),
                          rows->begin(),
                          row_window_size,
                          std::less<float>());
    }
  }
}

float AutoSegmentPeaks(const std::vector<float>& input) {
  // This is a thresholding operation based on Otsu's method.
  std::vector<int> histogram(256, 0);
  std::vector<float>::const_iterator it;

  float value_min = std::numeric_limits<float>::max();
  float value_max = std::numeric_limits<float>::min();

  for (it = input.begin(); it < input.end(); ++it) {
    value_min = std::min(value_min, *it);
    value_max = std::max(value_max, *it);
  }

  if (value_max - value_min <= std::numeric_limits<float>::epsilon() * 100) {
    // Scaling won't work and there is nothing really to segment anyway.
    return value_min;
  }

  float value_span = value_max - value_min;
  for (it = input.begin(); it < input.end(); ++it) {
    float scaled_value = (*it - value_min) / value_span * 255;
    histogram[static_cast<int>(scaled_value)] += 1;
  }

  // Otsu's method seeks to maximize variance between two classes of pixels
  // correspondng to valleys and peaks of the profile.
  double w1 = histogram[0];  // Total weight of the first class.
  double t1 = 0.5 * w1;
  double w2 = 0;
  double t2 = 0;
  for (size_t i = 1; i < histogram.size(); ++i) {
    w2 += histogram[i];
    t2 += (0.5 + i) * histogram[i];
  }

  size_t max_index = 0;
  double m1 = t1 / w1;
  double m2 = t2 / w2;
  double max_variance_score = w1 * w2 * (m1 - m2) * (m1 - m2);
  // Iterate through all possible ways of splitting the histogram.
  for (size_t i = 1; i < histogram.size() - 1; i++) {
    double bin_volume = (0.5 + i) * histogram[i];
    w1 += histogram[i];
    w2 -= histogram[i];
    t2 -= bin_volume;
    t1 += bin_volume;
    m1 = t1 / w1;
    m2 = t2 / w2;
    double variance_score = w1 * w2 * (m1 - m2) * (m1 - m2);
    if (variance_score >= max_variance_score) {
      max_variance_score = variance_score;
      max_index = i;
    }
  }

  // max_index refers to the bin *after* which we need to split. The sought
  // threshold is the centre of this bin, scaled back to the original range.
  return value_span * (max_index + 0.5f) / 255.0f + value_min;
}

SkBitmap ComputeDecimatedImage(const SkBitmap& bitmap,
                               const std::vector<bool>& rows,
                               const std::vector<bool>& columns) {
  SkAutoLockPixels source_lock(bitmap);
  DCHECK(bitmap.getPixels());
  DCHECK_GT(bitmap.bytesPerPixel(), 0);
  DCHECK_EQ(bitmap.width(), static_cast<int>(columns.size()));
  DCHECK_EQ(bitmap.height(), static_cast<int>(rows.size()));

  unsigned target_row_count = std::count(rows.begin(), rows.end(), true);
  unsigned target_column_count = std::count(
      columns.begin(), columns.end(), true);

  if (target_row_count == 0 || target_column_count == 0)
    return SkBitmap();  // Not quite an error, so no DCHECK. Just return empty.

  if (target_row_count == rows.size() && target_column_count == columns.size())
    return SkBitmap();  // Equivalent of the situation above (empty target).

  // Allocate the target image.
  SkBitmap target;
  target.setConfig(bitmap.config(), target_column_count, target_row_count);
  target.allocPixels();

  int target_row = 0;
  for (int r = 0; r < bitmap.height(); ++r) {
    if (!rows[r])
      continue;  // We can just skip this one.
    uint8* src_row =
        static_cast<uint8*>(bitmap.getPixels()) + r * bitmap.rowBytes();
    uint8* insertion_target = static_cast<uint8*>(target.getPixels()) +
        target_row * target.rowBytes();
    int left_copy_pixel = -1;
    for (int c = 0; c < bitmap.width(); ++c) {
      if (left_copy_pixel < 0 && columns[c]) {
        left_copy_pixel = c;  // Next time we will start copying from here.
      } else if (left_copy_pixel >= 0 && !columns[c]) {
        // This closes a fragment we want to copy. We do it now.
        size_t bytes_to_copy = (c - left_copy_pixel) * bitmap.bytesPerPixel();
        memcpy(insertion_target,
               src_row + left_copy_pixel * bitmap.bytesPerPixel(),
               bytes_to_copy);
        left_copy_pixel = -1;
        insertion_target += bytes_to_copy;
      }
    }
    // We can still have the tail end to process here.
    if (left_copy_pixel >= 0) {
      size_t bytes_to_copy =
          (bitmap.width() - left_copy_pixel) * bitmap.bytesPerPixel();
      memcpy(insertion_target,
             src_row + left_copy_pixel * bitmap.bytesPerPixel(),
             bytes_to_copy);
    }
    target_row++;
  }

  return target;
}

SkBitmap CreateRetargettedThumbnailImage(
    const SkBitmap& source_bitmap,
    const gfx::Size& target_size,
    float kernel_sigma) {
  // First thing we need for this method is to color-reduce the source_bitmap.
  SkBitmap reduced_color;
  reduced_color.setConfig(
      SkBitmap::kA8_Config, source_bitmap.width(), source_bitmap.height());
  reduced_color.allocPixels();

  if (!color_utils::ComputePrincipalComponentImage(source_bitmap,
                                                   &reduced_color)) {
    // CCIR601 luminance conversion vector.
    gfx::Vector3dF transform(0.299f, 0.587f, 0.114f);
    if (!color_utils::ApplyColorReduction(
            source_bitmap, transform, true, &reduced_color)) {
      DLOG(WARNING) << "Failed to compute luminance image from a screenshot. "
                    << "Cannot compute retargetted thumbnail.";
      return SkBitmap();
    }
    DLOG(WARNING) << "Could not compute principal color image for a thumbnail. "
                  << "Using luminance instead.";
  }

  // Turn 'color-reduced' image into the 'energy' image.
  ApplyGaussianGradientMagnitudeFilter(&reduced_color, kernel_sigma);

  // Extract vertical and horizontal projection of image features.
  std::vector<float> row_profile;
  std::vector<float> column_profile;
  ExtractImageProfileInformation(reduced_color,
                                 gfx::Rect(reduced_color.width(),
                                           reduced_color.height()),
                                 target_size,
                                 true,
                                 &row_profile,
                                 &column_profile);
  float threshold_rows = AutoSegmentPeaks(row_profile);
  float threshold_columns = AutoSegmentPeaks(column_profile);

  // Apply thresholding.
  std::vector<bool> included_rows(row_profile.size(), false);
  std::transform(row_profile.begin(),
                 row_profile.end(),
                 included_rows.begin(),
                 std::bind2nd(std::greater<float>(), threshold_rows));

  std::vector<bool> included_columns(column_profile.size(), false);
  std::transform(column_profile.begin(),
                 column_profile.end(),
                 included_columns.begin(),
                 std::bind2nd(std::greater<float>(), threshold_columns));

  // Use the original image and computed inclusion vectors to create a resized
  // image.
  return ComputeDecimatedImage(source_bitmap, included_rows, included_columns);
}

}  // thumbnailing_utils
