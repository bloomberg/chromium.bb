// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>

#include "base/numerics/checked_math.h"
#include "components/safe_browsing/password_protection/visual_utils.h"

namespace safe_browsing {
namespace visual_utils {

// A QuantizedColor takes the highest 3 bits of R, G, and B, and concatenates
// them.
QuantizedColor SkColorToQuantizedColor(SkColor color) {
  return (SkColorGetR(color) >> 5) << 6 | (SkColorGetG(color) >> 5) << 3 |
         (SkColorGetB(color) >> 5);
}

int GetQuantizedR(QuantizedColor color) {
  return color >> 6;
}

int GetQuantizedG(QuantizedColor color) {
  return (color >> 3) & 7;
}

int GetQuantizedB(QuantizedColor color) {
  return color & 7;
}

bool GetHistogramForImage(const SkBitmap& image,
                          VisualFeatures::ColorHistogram* histogram) {
  if (image.drawsNothing())
    return false;

  std::unordered_map<QuantizedColor, int> color_to_count;
  std::unordered_map<QuantizedColor, double> color_to_total_x;
  std::unordered_map<QuantizedColor, double> color_to_total_y;
  for (int x = 0; x < image.width(); x++) {
    for (int y = 0; y < image.height(); y++) {
      QuantizedColor color = SkColorToQuantizedColor(image.getColor(x, y));
      color_to_count[color]++;
      color_to_total_x[color] += static_cast<float>(x) / image.width();
      color_to_total_y[color] += static_cast<float>(y) / image.height();
    }
  }

  int normalization_factor;
  if (!base::CheckMul(image.width(), image.height())
           .AssignIfValid(&normalization_factor))
    return false;

  for (const auto& entry : color_to_count) {
    const QuantizedColor& color = entry.first;
    int count = entry.second;

    VisualFeatures::ColorHistogramBin* bin = histogram->add_bins();
    bin->set_weight(static_cast<float>(count) / normalization_factor);
    bin->set_centroid_x(color_to_total_x[color] / count);
    bin->set_centroid_y(color_to_total_y[color] / count);
    bin->set_quantized_r(GetQuantizedR(color));
    bin->set_quantized_g(GetQuantizedG(color));
    bin->set_quantized_b(GetQuantizedB(color));
  }

  return true;
}

}  // namespace visual_utils
}  // namespace safe_browsing
