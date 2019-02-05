// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <vector>

#include "components/safe_browsing/password_protection/visual_utils.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace safe_browsing {
namespace visual_utils {

namespace {

// WARNING: The following parameters are highly privacy and performance
// sensitive. These should not be changed without thorough review.
const int kPHashDownsampleWidth = 288;
const int kPHashDownsampleHeight = 288;
const int kPHashBlockSize = 6;

}  // namespace

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

bool GetBlurredImage(const SkBitmap& image,
                     VisualFeatures::BlurredImage* blurred_image) {
  if (image.drawsNothing())
    return false;

  // Use the Rec. 2020 color space, in case the user input is wide-gamut.
  sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
      {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
      SkNamedGamut::kRec2020);

  // We scale down twice, once with medium quality, once with none quality to be
  // consistent with the backend.
  // TODO(drubery): Investigate whether this is necessary for performance or
  // not.
  SkImageInfo downsampled_info =
      SkImageInfo::Make(kPHashDownsampleWidth, kPHashDownsampleHeight,
                        SkColorType::kRGBA_8888_SkColorType,
                        SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  SkBitmap downsampled;
  if (!downsampled.tryAllocPixels(downsampled_info))
    return false;
  image.pixmap().scalePixels(downsampled.pixmap(),
                             SkFilterQuality::kMedium_SkFilterQuality);

  SkImageInfo blurred_info =
      SkImageInfo::Make(kPHashDownsampleWidth / kPHashBlockSize,
                        kPHashDownsampleHeight / kPHashBlockSize,
                        SkColorType::kRGBA_8888_SkColorType,
                        SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  SkBitmap blurred;
  if (!blurred.tryAllocPixels(blurred_info))
    return false;
  downsampled.pixmap().scalePixels(blurred.pixmap(),
                                   SkFilterQuality::kNone_SkFilterQuality);

  blurred_image->set_width(blurred.width());
  blurred_image->set_height(blurred.height());
  blurred_image->clear_data();

  const uint32_t* rgba = blurred.getAddr32(0, 0);
  for (int i = 0; i < blurred.width() * blurred.height(); i++) {
    *blurred_image->mutable_data() += static_cast<char>((rgba[i] >> 0) & 0xff);
    *blurred_image->mutable_data() += static_cast<char>((rgba[i] >> 8) & 0xff);
    *blurred_image->mutable_data() += static_cast<char>((rgba[i] >> 16) & 0xff);
  }

  return true;
}

}  // namespace visual_utils
}  // namespace safe_browsing
