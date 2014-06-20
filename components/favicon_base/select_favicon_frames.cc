// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/select_favicon_frames.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

namespace {

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

SkBitmap SampleNearestNeighbor(const SkBitmap& contents, int desired_size) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, desired_size, desired_size);
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

enum ResizeMethod { NONE, SAMPLE_NEAREST_NEIGHBOUR, LANCZOS };

size_t GetCandidateIndexWithBestScore(
    const std::vector<gfx::Size>& candidate_sizes,
    int desired_size,
    float* score,
    ResizeMethod* resize_method) {
  DCHECK_NE(desired_size, 0);

  // Try to find an exact match.
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    if (candidate_sizes[i].width() == desired_size &&
        candidate_sizes[i].height() == desired_size) {
      *score = 1;
      *resize_method = NONE;
      return i;
    }
  }

  // Huge favicon bitmaps often have a completely different visual style from
  // smaller favicon bitmaps. Avoid them.
  const int kHugeEdgeSize = desired_size * 8;

  // Order of preference:
  // 1) Bitmaps with width and height smaller than |kHugeEdgeSize|.
  // 2) Bitmaps which need to be scaled down instead of up.
  // 3) Bitmaps which do not need to be scaled as much.
  size_t candidate_index = std::numeric_limits<size_t>::max();
  float candidate_score = 0;
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    float average_edge =
        (candidate_sizes[i].width() + candidate_sizes[i].height()) / 2.0f;

    float score = 0;
    if (candidate_sizes[i].width() >= kHugeEdgeSize ||
        candidate_sizes[i].height() >= kHugeEdgeSize) {
      score = std::min(1.0f, desired_size / average_edge) * 0.01f;
    } else if (candidate_sizes[i].width() >= desired_size &&
               candidate_sizes[i].height() >= desired_size) {
      score = desired_size / average_edge * 0.01f + 0.15f;
    } else {
      score = std::min(1.0f, average_edge / desired_size) * 0.01f + 0.1f;
    }

    if (candidate_index == std::numeric_limits<size_t>::max() ||
        score > candidate_score) {
      candidate_index = i;
      candidate_score = score;
    }
  }
  *score = candidate_score;

  // Integer multiples are built using nearest neighbor sampling. Otherwise,
  // Lanczos scaling is used.
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

// Represents the index of the best candidate for |desired_size| from the
// |candidate_sizes| passed into GetCandidateIndicesWithBestScores().
struct SelectionResult {
  // index in |candidate_sizes| of the best candidate.
  size_t index;

  // The desired size for which |index| is the best candidate.
  int desired_size;

  // How the bitmap data that the bitmap with |candidate_sizes[index]| should
  // be resized for displaying in the UI.
  ResizeMethod resize_method;
};

void GetCandidateIndicesWithBestScores(
    const std::vector<gfx::Size>& candidate_sizes,
    const std::vector<int>& desired_sizes,
    float* match_score,
    std::vector<SelectionResult>* results) {
  if (candidate_sizes.empty() || desired_sizes.empty()) {
    if (match_score)
      *match_score = 0.0f;
    return;
  }

  std::vector<int>::const_iterator zero_size_it =
      std::find(desired_sizes.begin(), desired_sizes.end(), 0);
  if (zero_size_it != desired_sizes.end()) {
    // Just return the biggest image available.
    SelectionResult result;
    result.index = BiggestCandidate(candidate_sizes);
    result.desired_size = 0;
    result.resize_method = NONE;
    results->push_back(result);
    if (match_score)
      *match_score = 1.0f;
    return;
  }

  float total_score = 0;
  for (size_t i = 0; i < desired_sizes.size(); ++i) {
    float score;
    SelectionResult result;
    result.desired_size = desired_sizes[i];
    result.index = GetCandidateIndexWithBestScore(
        candidate_sizes, result.desired_size, &score, &result.resize_method);
    results->push_back(result);
    total_score += score;
  }

  if (match_score)
    *match_score = total_score / desired_sizes.size();
}

// Resize |source_bitmap| using |resize_method|.
SkBitmap GetResizedBitmap(const SkBitmap& source_bitmap,
                          int desired_size,
                          ResizeMethod resize_method) {
  switch (resize_method) {
    case NONE:
      return source_bitmap;
    case SAMPLE_NEAREST_NEIGHBOUR:
      return SampleNearestNeighbor(source_bitmap, desired_size);
    case LANCZOS:
      return skia::ImageOperations::Resize(
          source_bitmap,
          skia::ImageOperations::RESIZE_LANCZOS3,
          desired_size,
          desired_size);
  }
  return source_bitmap;
}

}  // namespace

const float kSelectFaviconFramesInvalidScore = -1.0f;

gfx::ImageSkia SelectFaviconFrames(const std::vector<SkBitmap>& bitmaps,
                                   const std::vector<gfx::Size>& original_sizes,
                                   const std::vector<float>& favicon_scales,
                                   int desired_size_in_dip,
                                   float* match_score) {
  std::vector<int> desired_sizes;
  std::map<int, float> scale_map;
  if (desired_size_in_dip == 0) {
    desired_sizes.push_back(0);
    scale_map[0] = 1.0f;
  } else {
    for (size_t i = 0; i < favicon_scales.size(); ++i) {
      float scale = favicon_scales[i];
      int desired_size = ceil(desired_size_in_dip * scale);
      desired_sizes.push_back(desired_size);
      scale_map[desired_size] = scale;
    }
  }

  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(
      original_sizes, desired_sizes, match_score, &results);

  gfx::ImageSkia multi_image;
  for (size_t i = 0; i < results.size(); ++i) {
    const SelectionResult& result = results[i];
    SkBitmap resized_bitmap = GetResizedBitmap(
        bitmaps[result.index], result.desired_size, result.resize_method);

    std::map<int, float>::const_iterator it =
        scale_map.find(result.desired_size);
    DCHECK(it != scale_map.end());
    float scale = it->second;
    multi_image.AddRepresentation(gfx::ImageSkiaRep(resized_bitmap, scale));
  }
  return multi_image;
}

void SelectFaviconFrameIndices(const std::vector<gfx::Size>& frame_pixel_sizes,
                               const std::vector<int>& desired_sizes,
                               std::vector<size_t>* best_indices,
                               float* match_score) {
  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(
      frame_pixel_sizes, desired_sizes, match_score, &results);

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
