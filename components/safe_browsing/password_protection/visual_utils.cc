// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <vector>

#include "base/numerics/checked_math.h"
#include "components/safe_browsing/password_protection/visual_utils.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace safe_browsing {
namespace visual_utils {

namespace {

// Constants used in getting the luminance of a Rec 2020 RGB value.
// Drawn from:
// https://en.wikipedia.org/wiki/Rec._2020#RGB_and_luma-chroma_formats
const uint32_t kWeightRed = 263;
const uint32_t kWeightGreen = 678;
const uint32_t kWeightBlue = 59;

// WARNING: The following parameters are highly privacy and performance
// sensitive. These should not be changed without thorough review.
const int kPHashDownsampleWidth = 288;
const int kPHashDownsampleHeight = 288;
const int kPHashBlockSize = 6;

// Returns the median value of the list.
uint8_t GetMedian(const std::vector<uint8_t>& samples) {
  std::vector<uint8_t> samples_copy = samples;
  std::vector<uint8_t>::iterator middle =
      samples_copy.begin() + (samples_copy.size() / 2);
  std::nth_element(samples_copy.begin(), middle, samples_copy.end());
  return *middle;
}

// Encode the luminances as a bitstring, with a "1" bit if the luminance is
// above the cutoff, and a "0" if it's below.
void EncodeHash(const std::vector<uint8_t>& luminances,
                uint8_t cutoff_luminance,
                std::string* output) {
  int current_bits = 0;
  uint8_t current_byte = 0;
  *output = "";

  for (uint8_t luminance : luminances) {
    current_bits++;
    current_byte <<= 1;
    if (luminance >= cutoff_luminance)
      current_byte |= 1;

    if (current_bits == 8) {
      *output += static_cast<char>(current_byte);
      current_bits = 0;
      current_byte = 0;
    }
  }

  if (current_bits != 0) {
    current_byte <<= (8 - current_bits);
    *output += static_cast<char>(current_byte);
  }
}

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

// Computes the final PHash value for the BlurredImage. For each pixel in the
// blurred image, we compute its luminance. Then we create a bitstring, where
// each pixel gives a "1" if the luminance is at least the median, and a "0"
// otherwise.
bool GetPHash(const VisualFeatures::BlurredImage& blurred_image,
              std::string* phash) {
  DCHECK_LE(blurred_image.width(), kPHashDownsampleWidth);
  DCHECK_LE(blurred_image.height(), kPHashDownsampleHeight);

  int width = blurred_image.width();
  int height = blurred_image.height();

  base::CheckedNumeric<int> expected_size =
      base::CheckMul(3u, base::CheckMul(width, height));
  if (!expected_size.IsValid())
    return false;
  if (blurred_image.data().size() != expected_size.ValueOrDie())
    return false;

  std::vector<uint8_t> luminances;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int current_offset = 3 * width * y + 3 * x;
      uint8_t r = blurred_image.data()[current_offset];
      uint8_t g = blurred_image.data()[current_offset + 1];
      uint8_t b = blurred_image.data()[current_offset + 2];

      uint8_t luminance =
          (kWeightRed * r + kWeightGreen * g + kWeightBlue * b) /
          (kWeightRed + kWeightGreen + kWeightBlue);
      luminances.push_back(luminance);
    }
  }

  uint8_t cutoff_luminance = GetMedian(luminances);
  EncodeHash(luminances, cutoff_luminance, phash);
  return true;
}

}  // namespace visual_utils
}  // namespace safe_browsing
