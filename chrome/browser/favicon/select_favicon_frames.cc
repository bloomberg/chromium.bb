// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/select_favicon_frames.h"

#include "skia/ext/image_operations.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace {

size_t BiggestCandidate(const std::vector<SkBitmap>& bitmaps) {
  size_t max_index = 0;
  int max_area = bitmaps[0].width() * bitmaps[0].height();
  for (size_t i = 1; i < bitmaps.size(); ++i) {
    int area = bitmaps[i].width() * bitmaps[i].height();
    if (area > max_area) {
      max_area = area;
      max_index = i;
    }
  }
  return max_index;
}

SkBitmap PadWithBorder(const SkBitmap& contents,
                       int desired_size,
                       int source_size) {
  SkBitmap bitmap;
  bitmap.setConfig(
      SkBitmap::kARGB_8888_Config, desired_size, desired_size);
  bitmap.allocPixels();
  bitmap.eraseARGB(0, 0, 0, 0);

  {
    SkCanvas canvas(bitmap);
    int shift = (desired_size - source_size) / 2;
    SkRect dest(SkRect::MakeXYWH(shift, shift, source_size, source_size));
    canvas.drawBitmapRect(contents, NULL, dest);
  }

  return bitmap;
}

SkBitmap SampleNearestNeighbor(const SkBitmap& contents, int desired_size) {
  SkBitmap bitmap;
  bitmap.setConfig(
      SkBitmap::kARGB_8888_Config, desired_size, desired_size);
  bitmap.allocPixels();
  if (!contents.isOpaque())
    bitmap.eraseARGB(0, 0, 0, 0);

  {
    SkCanvas canvas(bitmap);
    SkRect dest(SkRect::MakeWH(desired_size, desired_size));
    canvas.drawBitmapRect(contents, NULL, dest);
  }

  return bitmap;
}

SkBitmap SelectCandidate(const std::vector<SkBitmap>& bitmaps,
                         int desired_size,
                         ui::ScaleFactor scale_factor,
                         float* score) {
  float scale = GetScaleFactorScale(scale_factor);
  desired_size = static_cast<int>(desired_size * scale + 0.5f);

  // Try to find an exact match.
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].width() == desired_size &&
        bitmaps[i].height() == desired_size) {
      *score = 1;
      return bitmaps[i];
    }
  }

  // If that failed, the following special rules apply:
  // 1. 17px-24px images are built from 16px images by adding
  //    a transparent border.
  if (desired_size > 16 * scale && desired_size <= 24 * scale) {
    int source_size = static_cast<int>(16 * scale + 0.5f);
    for (size_t i = 0; i < bitmaps.size(); ++i) {
      if (bitmaps[i].width() == source_size &&
          bitmaps[i].height() == source_size) {
        *score = 0.2f;
        return PadWithBorder(bitmaps[i], desired_size, source_size);
      }
    }
    // Try again, with upsizing the base variant.
    for (size_t i = 0; i < bitmaps.size(); ++i) {
      if (bitmaps[i].width() * scale == source_size &&
          bitmaps[i].height() * scale == source_size) {
        *score = 0.15f;
        return PadWithBorder(bitmaps[i], desired_size, source_size);
      }
    }
  }

  // 2. Integer multiples are built using nearest neighbor sampling.
  // 3. Else, use Lancosz scaling:
  //    b) If available, from the next bigger variant.
  int candidate = -1;
  int min_area = INT_MAX;
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    int area = bitmaps[i].width() * bitmaps[i].height();
    if (bitmaps[i].width() > desired_size &&
        bitmaps[i].height() > desired_size &&
        (candidate == -1 || area < min_area)) {
      candidate = i;
      min_area = area;
    }
  }
  *score = 0.1f;
  //    c) Else, from the biggest smaller variant.
  if (candidate == -1) {
    *score = 0;
    candidate = BiggestCandidate(bitmaps);
  }

  const SkBitmap& bitmap = bitmaps[candidate];
  bool is_integer_multiple = desired_size % bitmap.width() == 0 &&
                             desired_size % bitmap.height() == 0;
  if (is_integer_multiple)
    return SampleNearestNeighbor(bitmap, desired_size);
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      desired_size, desired_size);
}

}  // namespace

gfx::ImageSkia SelectFaviconFrames(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    float* match_score) {
  gfx::ImageSkia multi_image;
  if (bitmaps.empty())
    return multi_image;

  if (desired_size == 0) {
    // Just return the biggest image available.
    size_t max_index = BiggestCandidate(bitmaps);
    multi_image.AddRepresentation(
        gfx::ImageSkiaRep(bitmaps[max_index], ui::SCALE_FACTOR_100P));
    if (match_score)
      *match_score = 0.8f;
    return multi_image;
  }

  float total_score = 0;
  for (size_t i = 0; i < scale_factors.size(); ++i) {
    float score;
    multi_image.AddRepresentation(gfx::ImageSkiaRep(
          SelectCandidate(bitmaps, desired_size, scale_factors[i], &score),
          scale_factors[i]));
    total_score += score;
  }

  if (match_score)
    *match_score = total_score / scale_factors.size();
  return multi_image;
}
