// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_util.h"

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/select_favicon_frames.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

// static
std::vector<ui::ScaleFactor> FaviconUtil::GetFaviconScaleFactors() {
  const float kScale1x = ui::GetScaleFactorScale(ui::SCALE_FACTOR_100P);
  std::vector<ui::ScaleFactor> favicon_scale_factors =
      ui::GetSupportedScaleFactors();

  // The scale factors returned from ui::GetSupportedScaleFactors() are sorted.
  // Insert the 1x scale factor such that GetFaviconScaleFactors() is sorted as
  // well.
  size_t insert_index = favicon_scale_factors.size();
  for (size_t i = 0; i < favicon_scale_factors.size(); ++i) {
    float scale = ui::GetScaleFactorScale(favicon_scale_factors[i]);
    if (scale == kScale1x) {
      return favicon_scale_factors;
    } else if (scale > kScale1x) {
      insert_index = i;
      break;
    }
  }
  favicon_scale_factors.insert(favicon_scale_factors.begin() + insert_index,
                               ui::SCALE_FACTOR_100P);
  return favicon_scale_factors;
}

// static
gfx::Image FaviconUtil::SelectFaviconFramesFromPNGs(
      const std::vector<history::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor> scale_factors,
      int favicon_size) {
  std::vector<SkBitmap> bitmaps;
  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      continue;

    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(png_data[i].bitmap_data->front(),
                              png_data[i].bitmap_data->size(),
                              &bitmap)) {
      bitmaps.push_back(bitmap);
    }
  }

  if (bitmaps.empty())
    return gfx::Image();

  gfx::ImageSkia resized_image_skia = SelectFaviconFrames(bitmaps,
      scale_factors, favicon_size, NULL);
  return gfx::Image(resized_image_skia);
}

// static
size_t FaviconUtil::SelectBestFaviconFromBitmaps(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size) {
  std::vector<gfx::Size> sizes;
  for (size_t i = 0; i < bitmaps.size(); ++i)
    sizes.push_back(gfx::Size(bitmaps[i].width(), bitmaps[i].height()));
  std::vector<size_t> selected_bitmap_indices;
  SelectFaviconFrameIndices(sizes, scale_factors, desired_size,
                            &selected_bitmap_indices, NULL);
  DCHECK_EQ(1u, selected_bitmap_indices.size());
  return selected_bitmap_indices[0];
}
