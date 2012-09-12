// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/select_favicon_frames.h"

#include <set>

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

namespace {

// Return gfx::Size vector with the pixel sizes of |bitmaps|.
void SizesFromBitmaps(const std::vector<SkBitmap>& bitmaps,
                      std::vector<gfx::Size>* sizes) {
  for (size_t i = 0; i < bitmaps.size(); ++i)
    sizes->push_back(gfx::Size(bitmaps[i].width(), bitmaps[i].height()));
}

size_t BiggestCandidate(const std::vector<gfx::Size>& candidate_sizes) {
  size_t max_index = 0;
  int max_area = candidate_sizes[0].GetArea();
  for (size_t i = 1; i < candidate_sizes.size(); ++i) {
    int area = candidate_sizes[i].GetArea();
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

enum ResizeMethod {
NONE,
PAD_WITH_BORDER,
SAMPLE_NEAREST_NEIGHBOUR,
LANCZOS
};

size_t GetCandidateIndexWithBestScore(
    const std::vector<gfx::Size>& candidate_sizes,
    ui::ScaleFactor scale_factor,
    int desired_size,
    float* score,
    ResizeMethod* resize_method) {
  float scale = ui::GetScaleFactorScale(scale_factor);
  desired_size = static_cast<int>(desired_size * scale + 0.5f);

  // Try to find an exact match.
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    if (candidate_sizes[i].width() == desired_size &&
        candidate_sizes[i].height() == desired_size) {
      *score = 1;
      *resize_method = NONE;
      return i;
    }
  }

  // If that failed, the following special rules apply:
  // 1. 17px-24px images are built from 16px images by adding
  //    a transparent border.
  if (desired_size > 16 * scale && desired_size <= 24 * scale) {
    int source_size = static_cast<int>(16 * scale + 0.5f);
    for (size_t i = 0; i < candidate_sizes.size(); ++i) {
      if (candidate_sizes[i].width() == source_size &&
          candidate_sizes[i].height() == source_size) {
        *score = 0.2f;
        *resize_method = PAD_WITH_BORDER;
        return i;
      }
    }
    // Try again, with upsizing the base variant.
    for (size_t i = 0; i < candidate_sizes.size(); ++i) {
      if (candidate_sizes[i].width() * scale == source_size &&
          candidate_sizes[i].height() * scale == source_size) {
        *score = 0.15f;
        *resize_method = PAD_WITH_BORDER;
        return i;
      }
    }
  }

  // 2. Integer multiples are built using nearest neighbor sampling.
  // 3. Else, use Lancosz scaling:
  //    b) If available, from the next bigger variant.
  int candidate_index = -1;
  int min_area = INT_MAX;
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    int area = candidate_sizes[i].GetArea();
    if (candidate_sizes[i].width() > desired_size &&
        candidate_sizes[i].height() > desired_size &&
        (candidate_index == -1 || area < min_area)) {
      candidate_index = i;
      min_area = area;
    }
  }
  *score = 0.1f;
  //    c) Else, from the biggest smaller variant.
  if (candidate_index == -1) {
    *score = 0;
    candidate_index = BiggestCandidate(candidate_sizes);
  }

  const gfx::Size& candidate_size = candidate_sizes[candidate_index];
  if (candidate_size.IsEmpty()) {
    *resize_method = NONE;
  } else if (desired_size % candidate_size.width() == 0 &&
             desired_size % candidate_size.height() == 0) {
    *resize_method = SAMPLE_NEAREST_NEIGHBOUR;
  } else {
    *resize_method = LANCZOS;
  }
  return candidate_index;
}

// Represents the index of the best candidate for a |scale_factor| from the
// |candidate_sizes| passed into GetCandidateIndicesWithBestScores().
struct SelectionResult {
  // index in |candidate_sizes| of the best candidate.
  size_t index;

  // The ScaleFactor for which |index| is the best candidate.
  ui::ScaleFactor scale_factor;

  // How the bitmap data that the bitmap with |candidate_sizes[index]| should
  // be resized for displaying in the UI.
  ResizeMethod resize_method;
};

void GetCandidateIndicesWithBestScores(
    const std::vector<gfx::Size>& candidate_sizes,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    float* match_score,
    std::vector<SelectionResult>* results) {
  if (candidate_sizes.empty()) {
    *match_score = 0.0f;
    return;
  }

  if (desired_size == 0) {
    // Just return the biggest image available.
    SelectionResult result;
    result.index = BiggestCandidate(candidate_sizes);
    result.scale_factor = ui::SCALE_FACTOR_100P;
    result.resize_method = NONE;
    results->push_back(result);
    if (match_score)
      *match_score = 0.8f;
    return;
  }

  float total_score = 0;
  for (size_t i = 0; i < scale_factors.size(); ++i) {
    float score;
    SelectionResult result;
    result.scale_factor = scale_factors[i];
    result.index = GetCandidateIndexWithBestScore(candidate_sizes,
        result.scale_factor, desired_size, &score, &result.resize_method);
    results->push_back(result);
    total_score += score;
  }

  if (match_score)
    *match_score = total_score / scale_factors.size();
}

// Resize |source_bitmap| using |resize_method|.
SkBitmap GetResizedBitmap(const SkBitmap& source_bitmap,
                          int desired_size_in_dip,
                          ui::ScaleFactor scale_factor,
                          ResizeMethod resize_method) {
  float scale = ui::GetScaleFactorScale(scale_factor);
  int desired_size_in_pixel = static_cast<int>(
      desired_size_in_dip * scale + 0.5f);

  switch(resize_method) {
    case NONE:
      return source_bitmap;
    case PAD_WITH_BORDER: {
      int inner_border_in_pixel = static_cast<int>(16 * scale + 0.5f);
      return PadWithBorder(source_bitmap, desired_size_in_pixel,
                           inner_border_in_pixel);
    }
    case SAMPLE_NEAREST_NEIGHBOUR:
      return SampleNearestNeighbor(source_bitmap, desired_size_in_pixel);
    case LANCZOS:
      return skia::ImageOperations::Resize(
          source_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
          desired_size_in_pixel, desired_size_in_pixel);
  }
  return source_bitmap;
}

}  // namespace

const float kSelectFaviconFramesInvalidScore = -1.0f;

gfx::ImageSkia SelectFaviconFrames(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    float* match_score) {
  std::vector<gfx::Size> candidate_sizes;
  SizesFromBitmaps(bitmaps, &candidate_sizes);

  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(candidate_sizes, scale_factors,
      desired_size, match_score, &results);

  gfx::ImageSkia multi_image;
  for (size_t i = 0; i < results.size(); ++i) {
    const SelectionResult& result = results[i];
    SkBitmap resized_bitmap = GetResizedBitmap(bitmaps[result.index],
        desired_size, result.scale_factor, result.resize_method);
    multi_image.AddRepresentation(
        gfx::ImageSkiaRep(resized_bitmap, result.scale_factor));
  }
  return multi_image;
}

void SelectFaviconFrameIndices(
    const std::vector<gfx::Size>& frame_pixel_sizes,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    std::vector<size_t>* best_indices,
    float* match_score) {
  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(frame_pixel_sizes, scale_factors,
      desired_size, match_score, &results);

  std::set<size_t> already_added;
  for (size_t i = 0; i < results.size(); ++i) {
    size_t index = results[i].index;
    // GetCandidateIndicesWithBestScores() will return duplicate indices if the
    // bitmap data with |frame_pixel_sizes[index]| should be used for multiple
    // scale factors. Remove duplicates here such that |best_indices| contains
    // no duplicates.
    if (already_added.find(index) == already_added.end()) {
      already_added.insert(index);
      best_indices->push_back(index);
    }
  }
}
