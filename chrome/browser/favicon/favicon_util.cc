// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_util.h"

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/select_favicon_frames.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Attempts to create a resulting image of DIP size |favicon_size| with
// image reps of |scale_factors| without resizing or decoding the bitmap
// data.
// Returns an empty gfx::Image if the bitmap data must be resized.
gfx::Image SelectFaviconFramesFromPNGsWithoutResizing(
    const std::vector<history::FaviconBitmapResult>& png_data,
    const std::vector<ui::ScaleFactor> scale_factors,
    int favicon_size) {
  if (png_data.size() != scale_factors.size())
    return gfx::Image();

  // A |favicon_size| of 0 indicates that the largest frame is desired.
  if (favicon_size == 0 && png_data.size() == 1) {
    scoped_refptr<base::RefCountedMemory> bitmap_data = png_data[0].bitmap_data;
    return gfx::Image::CreateFrom1xPNGBytes(bitmap_data->front(),
                                            bitmap_data->size());
  }

  // Cache the scale factor for each pixel size as |scale_factors| may contain
  // any of GetFaviconScaleFactors() which may include scale factors not
  // supported by the platform. (ui::GetScaleFactorFromScale() cannot be used.)
  std::map<int, ui::ScaleFactor> desired_pixel_sizes;
  for (size_t i = 0; i < scale_factors.size(); ++i) {
    int pixel_size = floor(favicon_size *
        ui::GetScaleFactorScale(scale_factors[i]));
    desired_pixel_sizes[pixel_size] = scale_factors[i];
  }

  std::vector<gfx::ImagePNGRep> png_reps;
  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      return gfx::Image();

    const gfx::Size& pixel_size = png_data[i].pixel_size;
    if (pixel_size.width() != pixel_size.height())
      return gfx::Image();

    std::map<int, ui::ScaleFactor>::iterator it = desired_pixel_sizes.find(
        pixel_size.width());
    if (it == desired_pixel_sizes.end())
      return gfx::Image();

    png_reps.push_back(gfx::ImagePNGRep(png_data[i].bitmap_data, it->second));
  }

  return gfx::Image(png_reps);
}

}  // namespace

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
  // TODO(ios): 100p should not be necessary on iOS retina devices. However
  // the sync service only supports syncing 100p favicons. Until sync supports
  // other scales 100p is needed in the list of scale factors to retrieve and
  // store the favicons in both 100p for sync and 200p for display. cr/160503.
  favicon_scale_factors.insert(favicon_scale_factors.begin() + insert_index,
                               ui::SCALE_FACTOR_100P);
  return favicon_scale_factors;
}

// static
gfx::Image FaviconUtil::SelectFaviconFramesFromPNGs(
      const std::vector<history::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor> scale_factors,
      int favicon_size) {
  // Attempt to create a result image without resizing the bitmap data or
  // decoding it. FaviconHandler stores already resized favicons into history
  // so no additional resizing should be needed in the common case. Skipping
  // decoding to ImageSkia is useful because the ImageSkia representation may
  // not be needed and because the decoding is done on the UI thread and the
  // decoding can be a significant performance hit if a user has many bookmarks.
  // TODO(pkotwicz): Move the decoding off the UI thread.
  gfx::Image without_resizing = SelectFaviconFramesFromPNGsWithoutResizing(
      png_data, scale_factors, favicon_size);
  if (!without_resizing.IsEmpty())
    return without_resizing;

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
