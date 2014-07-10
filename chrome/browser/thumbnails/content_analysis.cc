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
#include "skia/ext/recursive_gaussian_convolution.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/color_analysis.h"

namespace {

const float kSigmaThresholdForRecursive = 1.5f;
const float kAspectRatioToleranceFactor = 1.02f;

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

size_t FindOtsuThresholdingIndex(const std::vector<int>& histogram) {
  // Otsu's method seeks to maximize variance between two classes of pixels
  // correspondng to valleys and peaks of the profile.
  double w1 = histogram[0];  // Total weight of the first class.
  double t1 = 0.5 * w1;
  double w2 = 0.0;
  double t2 = 0.0;
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

  return max_index;
}

bool ComputeScaledHistogram(const std::vector<float>& source,
                            std::vector<int>* histogram,
                            std::pair<float, float>* minmax) {
  DCHECK(histogram);
  DCHECK(minmax);
  histogram->clear();
  histogram->resize(256);
  float value_min = std::numeric_limits<float>::max();
  float value_max = 0.0f;

  std::vector<float>::const_iterator it;
  for (it = source.begin(); it < source.end(); ++it) {
    value_min = std::min(value_min, *it);
    value_max = std::max(value_max, *it);
  }

  *minmax = std::make_pair(value_min, value_max);

  if (value_max - value_min <= std::numeric_limits<float>::epsilon() * 100.0f) {
    // Scaling won't work and there is nothing really to segment anyway.
    return false;
  }

  float value_span = value_max - value_min;
  float scale = 255.0f / value_span;
  for (it = source.begin(); it < source.end(); ++it) {
    float scaled_value = (*it - value_min) * scale;
    (*histogram)[static_cast<int>(scaled_value)] += 1;
  }
  return true;
}

void ConstrainedProfileThresholding(const std::vector<float>& profile,
                                    const std::vector<int>& histogram,
                                    int current_clip_index,
                                    float current_threshold,
                                    const std::pair<float, float>& range,
                                    int size_for_threshold,
                                    int target_size,
                                    std::vector<bool>* result) {
  DCHECK(!profile.empty());
  DCHECK_EQ(histogram.size(), 256U);
  DCHECK(result);

  // A subroutine performing thresholding on the |profile|.
  if (size_for_threshold != target_size) {
    // Find a cut-off point (on the histogram) closest to the desired size.
    int candidate_size = profile.size();
    int candidate_clip_index = 0;
    for (std::vector<int>::const_iterator it = histogram.begin();
         it != histogram.end(); ++it, ++candidate_clip_index) {
      if (std::abs(candidate_size - target_size) <
          std::abs(candidate_size - *it - target_size)) {
        break;
      }
      candidate_size -= *it;
    }

    if (std::abs(candidate_size - target_size) <
        std::abs(candidate_size -size_for_threshold)) {
      current_clip_index = candidate_clip_index;
      current_threshold =  (range.second - range.first) *
          current_clip_index / 255.0f + range.first;
      // Recount, rather than assume. One-offs due to rounding can be very
      // harmful when eroding / dilating the result.
      size_for_threshold = std::count_if(
          profile.begin(), profile.end(),
          std::bind2nd(std::greater<float>(), current_threshold));
    }
  }

  result->resize(profile.size());
  for (size_t i = 0; i < profile.size(); ++i)
    (*result)[i] = profile[i] > current_threshold;

  while (size_for_threshold > target_size) {
    // If the current size is larger than target size, erode exactly as needed.
    std::vector<bool>::iterator mod_it = result->begin();
    std::vector<bool>::const_iterator lead_it = result->begin();
    bool prev_value = true;
    for (++lead_it;
         lead_it < result->end() && size_for_threshold > target_size;
         ++lead_it, ++mod_it) {
      bool value = *mod_it;
      // If any neighbour is false, switch the element off.
      if (!prev_value || !*lead_it) {
        *mod_it = false;
        --size_for_threshold;
      }
      prev_value = value;
    }

    if (lead_it == result->end() && !prev_value) {
      *mod_it = false;
      --size_for_threshold;
    }
  }

  while (size_for_threshold < target_size) {
    std::vector<bool>::iterator mod_it = result->begin();
    std::vector<bool>::const_iterator lead_it = result->begin();
    bool prev_value = false;
    for (++lead_it;
         lead_it < result->end() && size_for_threshold < target_size;
         ++lead_it, ++mod_it) {
      bool value = *mod_it;
      // If any neighbour is false, switch the element off.
      if (!prev_value || !*lead_it) {
        *mod_it = true;
        ++size_for_threshold;
      }
      prev_value = value;
    }

    if (lead_it == result->end() && !prev_value) {
      *mod_it = true;
      ++size_for_threshold;
    }
  }
}

}  // namespace

namespace thumbnailing_utils {

void ApplyGaussianGradientMagnitudeFilter(SkBitmap* input_bitmap,
                                          float kernel_sigma) {
  // The purpose of this function is to highlight salient
  // (attention-attracting?) features of the image for use in image
  // retargeting.
  SkAutoLockPixels source_lock(*input_bitmap);
  DCHECK(input_bitmap);
  DCHECK(input_bitmap->getPixels());
  DCHECK_EQ(kAlpha_8_SkColorType, input_bitmap->colorType());

  // To perform computations we will need one intermediate buffer. It can
  // very well be just another bitmap.
  const SkISize image_size = SkISize::Make(input_bitmap->width(),
                                           input_bitmap->height());
  SkBitmap intermediate;
  intermediate.allocPixels(input_bitmap->info().makeWH(image_size.width(),
                                                       image_size.height()));

  SkBitmap intermediate2;
  intermediate2.allocPixels(input_bitmap->info().makeWH(image_size.width(),
                                                        image_size.height()));

  if (kernel_sigma <= kSigmaThresholdForRecursive) {
    // For small kernels classic implementation is faster.
    skia::ConvolutionFilter1D smoothing_filter;
    skia::SetUpGaussianConvolutionKernel(
        &smoothing_filter, kernel_sigma, false);
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

    skia::ConvolutionFilter1D gradient_filter;
    skia::SetUpGaussianConvolutionKernel(&gradient_filter, kernel_sigma, true);
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
  } else {
    // For larger sigma values use the recursive filter.
    skia::RecursiveFilter smoothing_filter(kernel_sigma,
                                           skia::RecursiveFilter::FUNCTION);
    skia::SingleChannelRecursiveGaussianX(
        input_bitmap->getAddr8(0, 0),
        static_cast<int>(input_bitmap->rowBytes()),
        0, input_bitmap->bytesPerPixel(),
        smoothing_filter,
        image_size,
        intermediate.getAddr8(0, 0),
        static_cast<int>(intermediate.rowBytes()),
        0, intermediate.bytesPerPixel(), false);
    unsigned char smoothed_max = skia::SingleChannelRecursiveGaussianY(
        intermediate.getAddr8(0, 0),
        static_cast<int>(intermediate.rowBytes()),
        0, intermediate.bytesPerPixel(),
        smoothing_filter,
        image_size,
        input_bitmap->getAddr8(0, 0),
        static_cast<int>(input_bitmap->rowBytes()),
        0, input_bitmap->bytesPerPixel(), false);
    if (smoothed_max < 127) {
      int bit_shift = 8 - static_cast<int>(
          std::log10(static_cast<float>(smoothed_max)) / std::log10(2.0f));
      for (int r = 0; r < image_size.height(); ++r) {
        uint8* row = input_bitmap->getAddr8(0, r);
        for (int c = 0; c < image_size.width(); ++c, ++row) {
          *row <<= bit_shift;
        }
      }
    }

    skia::RecursiveFilter gradient_filter(
        kernel_sigma, skia::RecursiveFilter::FIRST_DERIVATIVE);
    skia::SingleChannelRecursiveGaussianX(
        input_bitmap->getAddr8(0, 0),
        static_cast<int>(input_bitmap->rowBytes()),
        0, input_bitmap->bytesPerPixel(),
        gradient_filter,
        image_size,
        intermediate.getAddr8(0, 0),
        static_cast<int>(intermediate.rowBytes()),
        0, intermediate.bytesPerPixel(), true);
    skia::SingleChannelRecursiveGaussianY(
        input_bitmap->getAddr8(0, 0),
        static_cast<int>(input_bitmap->rowBytes()),
        0, input_bitmap->bytesPerPixel(),
        gradient_filter,
        image_size,
        intermediate2.getAddr8(0, 0),
        static_cast<int>(intermediate2.rowBytes()),
        0, intermediate2.bytesPerPixel(), true);
  }

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
    const uint8* grad_x_row = intermediate.getAddr8(0, r);
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
  DCHECK_EQ(kAlpha_8_SkColorType, input_bitmap.colorType());
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
  std::vector<int> histogram;
  std::pair<float, float> minmax;
  if (!ComputeScaledHistogram(input, &histogram, &minmax))
    return minmax.first;

  // max_index refers to the bin *after* which we need to split. The sought
  // threshold is the centre of this bin, scaled back to the original range.
  size_t max_index = FindOtsuThresholdingIndex(histogram);
  return (minmax.second - minmax.first) * (max_index + 0.5f) / 255.0f +
      minmax.first;
}

gfx::Size AdjustClippingSizeToAspectRatio(const gfx::Size& target_size,
                                          const gfx::Size& image_size,
                                          const gfx::Size& computed_size) {
  DCHECK_GT(target_size.width(), 0);
  DCHECK_GT(target_size.height(), 0);
  // If the computed thumbnail would be too wide or to tall, we shall attempt
  // to fix it. Generally the idea is to re-add content to the part which has
  // been more aggressively shrunk unless there is nothing to add there or if
  // adding there won't fix anything. Should that be the case,  we will
  // (reluctantly) take away more from the other dimension.
  float desired_aspect =
      static_cast<float>(target_size.width()) / target_size.height();
  int computed_width = std::max(computed_size.width(), target_size.width());
  int computed_height = std::max(computed_size.height(), target_size.height());
  float computed_aspect = static_cast<float>(computed_width) / computed_height;
  float aspect_change_delta = std::abs(computed_aspect - desired_aspect);
  float prev_aspect_change_delta = 1000.0f;
  const float kAspectChangeEps = 0.01f;
  const float kLargeEffect = 2.0f;

  while ((prev_aspect_change_delta - aspect_change_delta > kAspectChangeEps) &&
         (computed_aspect / desired_aspect > kAspectRatioToleranceFactor ||
          desired_aspect / computed_aspect > kAspectRatioToleranceFactor)) {
    int new_computed_width = computed_width;
    int new_computed_height = computed_height;
    float row_dimension_shrink =
        static_cast<float>(image_size.height()) / computed_height;
    float column_dimension_shrink =
        static_cast<float>(image_size.width()) / computed_width;

    if (computed_aspect / desired_aspect > kAspectRatioToleranceFactor) {
      // Too wide.
      if (row_dimension_shrink > column_dimension_shrink) {
        // Bring the computed_height to the least of:
        // (1) image height (2) the number of lines that would
        // make up the desired aspect or (3) number of lines we would get
        // at the same 'aggressivity' level as width or.
        new_computed_height = std::min(
            static_cast<int>(image_size.height()),
            static_cast<int>(computed_width / desired_aspect + 0.5f));
        new_computed_height = std::min(
            new_computed_height,
            static_cast<int>(
                image_size.height() / column_dimension_shrink + 0.5f));
      } else if (row_dimension_shrink >= kLargeEffect ||
                 new_computed_width <= target_size.width()) {
        // Even though rows were resized less, we will generally rather add than
        // remove (or there is nothing to remove in x already).
        new_computed_height = std::min(
            static_cast<int>(image_size.height()),
            static_cast<int>(computed_width / desired_aspect + 0.5f));
      } else {
        // Rows were already shrunk less aggressively. This means there is
        // simply no room left too expand. Cut columns to get the desired
        // aspect ratio.
        new_computed_width = desired_aspect * computed_height + 0.5f;
      }
    } else {
      // Too tall.
      if (column_dimension_shrink > row_dimension_shrink) {
        // Columns were shrunk more aggressively. Try to relax the same way as
        // above.
        new_computed_width = std::min(
            static_cast<int>(image_size.width()),
            static_cast<int>(desired_aspect * computed_height + 0.5f));
        new_computed_width = std::min(
            new_computed_width,
            static_cast<int>(
                image_size.width() / row_dimension_shrink  + 0.5f));
      } else if (column_dimension_shrink  >= kLargeEffect ||
                 new_computed_height <= target_size.height()) {
        new_computed_width = std::min(
            static_cast<int>(image_size.width()),
            static_cast<int>(desired_aspect * computed_height + 0.5f));
      } else {
        new_computed_height = computed_width / desired_aspect + 0.5f;
      }
    }

    new_computed_width = std::max(new_computed_width, target_size.width());
    new_computed_height = std::max(new_computed_height, target_size.height());

    // Update loop control variables.
    float new_computed_aspect =
        static_cast<float>(new_computed_width) / new_computed_height;

    if (std::abs(new_computed_aspect - desired_aspect) >
        std::abs(computed_aspect - desired_aspect)) {
      // Do not take inferior results.
      break;
    }

    computed_width = new_computed_width;
    computed_height = new_computed_height;
    computed_aspect = new_computed_aspect;
    prev_aspect_change_delta = aspect_change_delta;
    aspect_change_delta = std::abs(new_computed_aspect - desired_aspect);
  }

  return gfx::Size(computed_width, computed_height);
}

void ConstrainedProfileSegmentation(const std::vector<float>& row_profile,
                                    const std::vector<float>& column_profile,
                                    const gfx::Size& target_size,
                                    std::vector<bool>* included_rows,
                                    std::vector<bool>* included_columns) {
  DCHECK(included_rows);
  DCHECK(included_columns);

  std::vector<int> histogram_rows;
  std::pair<float, float> minmax_rows;
  bool rows_well_behaved = ComputeScaledHistogram(
      row_profile, &histogram_rows, &minmax_rows);

  float row_threshold = minmax_rows.first;
  size_t clip_index_rows = 0;

  if (rows_well_behaved) {
    clip_index_rows = FindOtsuThresholdingIndex(histogram_rows);
    row_threshold = (minmax_rows.second - minmax_rows.first) *
        (clip_index_rows + 0.5f) / 255.0f + minmax_rows.first;
  }

  std::vector<int> histogram_columns;
  std::pair<float, float> minmax_columns;
  bool columns_well_behaved = ComputeScaledHistogram(column_profile,
                                                     &histogram_columns,
                                                     &minmax_columns);
  float column_threshold = minmax_columns.first;
  size_t clip_index_columns = 0;

  if (columns_well_behaved) {
    clip_index_columns = FindOtsuThresholdingIndex(histogram_columns);
    column_threshold = (minmax_columns.second - minmax_columns.first) *
        (clip_index_columns + 0.5f) / 255.0f + minmax_columns.first;
  }

  int auto_segmented_width = count_if(
      column_profile.begin(), column_profile.end(),
      std::bind2nd(std::greater<float>(), column_threshold));
  int auto_segmented_height = count_if(
      row_profile.begin(), row_profile.end(),
      std::bind2nd(std::greater<float>(), row_threshold));

  gfx::Size computed_size = AdjustClippingSizeToAspectRatio(
      target_size,
      gfx::Size(column_profile.size(), row_profile.size()),
      gfx::Size(auto_segmented_width, auto_segmented_height));

  // Apply thresholding.
  if (rows_well_behaved) {
    ConstrainedProfileThresholding(row_profile,
                                   histogram_rows,
                                   clip_index_rows,
                                   row_threshold,
                                   minmax_rows,
                                   auto_segmented_height,
                                   computed_size.height(),
                                   included_rows);
  } else {
    // This is essentially an error condition, invoked when no segmentation was
    // possible. This will result in applying a very low threshold and likely
    // in producing a thumbnail which should get rejected.
    included_rows->resize(row_profile.size());
    for (size_t i = 0; i < row_profile.size(); ++i)
      (*included_rows)[i] = row_profile[i] > row_threshold;
  }

  if (columns_well_behaved) {
    ConstrainedProfileThresholding(column_profile,
                                   histogram_columns,
                                   clip_index_columns,
                                   column_threshold,
                                   minmax_columns,
                                   auto_segmented_width,
                                   computed_size.width(),
                                   included_columns);
  } else {
    included_columns->resize(column_profile.size());
    for (size_t i = 0; i < column_profile.size(); ++i)
      (*included_columns)[i] = column_profile[i] > column_threshold;
  }
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
  target.allocPixels(bitmap.info().makeWH(target_column_count,
                                          target_row_count));

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

SkBitmap CreateRetargetedThumbnailImage(
    const SkBitmap& source_bitmap,
    const gfx::Size& target_size,
    float kernel_sigma) {
  // First thing we need for this method is to color-reduce the source_bitmap.
  SkBitmap reduced_color;
  reduced_color.allocPixels(SkImageInfo::MakeA8(source_bitmap.width(),
                                                source_bitmap.height()));

  if (!color_utils::ComputePrincipalComponentImage(source_bitmap,
                                                   &reduced_color)) {
    // CCIR601 luminance conversion vector.
    gfx::Vector3dF transform(0.299f, 0.587f, 0.114f);
    if (!color_utils::ApplyColorReduction(
            source_bitmap, transform, true, &reduced_color)) {
      DLOG(WARNING) << "Failed to compute luminance image from a screenshot. "
                    << "Cannot compute retargeted thumbnail.";
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

  std::vector<bool> included_rows, included_columns;
  ConstrainedProfileSegmentation(row_profile,
                                 column_profile,
                                 target_size,
                                 &included_rows,
                                 &included_columns);

  // Use the original image and computed inclusion vectors to create a resized
  // image.
  return ComputeDecimatedImage(source_bitmap, included_rows, included_columns);
}

}  // thumbnailing_utils
