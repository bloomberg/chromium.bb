// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_util.h"

#include "chrome/browser/history/select_favicon_frames.h"
#include "chrome/common/favicon/favicon_types.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

namespace {

// Creates image reps of DIP size |favicon_size| for the subset of
// |scale_factors| for which the image reps can be created without resizing
// or decoding the bitmap data.
std::vector<gfx::ImagePNGRep> SelectFaviconFramesFromPNGsWithoutResizing(
    const std::vector<chrome::FaviconBitmapResult>& png_data,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int favicon_size) {
  std::vector<gfx::ImagePNGRep> png_reps;
  if (png_data.empty())
    return png_reps;

  // A |favicon_size| of 0 indicates that the largest frame is desired.
  if (favicon_size == 0) {
    int maximum_area = 0;
    scoped_refptr<base::RefCountedMemory> best_candidate;
    for (size_t i = 0; i < png_data.size(); ++i) {
      int area = png_data[i].pixel_size.GetArea();
      if (area > maximum_area) {
        maximum_area = area;
        best_candidate = png_data[i].bitmap_data;
      }
    }
    png_reps.push_back(gfx::ImagePNGRep(best_candidate, 1.0f));
    return png_reps;
  }

  // Cache the scale factor for each pixel size as |scale_factors| may contain
  // any of GetFaviconScaleFactors() which may include scale factors not
  // supported by the platform. (ui::GetSupportedScaleFactor() cannot be used.)
  std::map<int, ui::ScaleFactor> desired_pixel_sizes;
  for (size_t i = 0; i < scale_factors.size(); ++i) {
    int pixel_size = floor(favicon_size *
        ui::GetImageScale(scale_factors[i]));
    desired_pixel_sizes[pixel_size] = scale_factors[i];
  }

  for (size_t i = 0; i < png_data.size(); ++i) {
    if (!png_data[i].is_valid())
      continue;

    const gfx::Size& pixel_size = png_data[i].pixel_size;
    if (pixel_size.width() != pixel_size.height())
      continue;

    std::map<int, ui::ScaleFactor>::iterator it = desired_pixel_sizes.find(
        pixel_size.width());
    if (it == desired_pixel_sizes.end())
      continue;

    png_reps.push_back(
        gfx::ImagePNGRep(png_data[i].bitmap_data,
                         ui::GetImageScale(it->second)));
  }

  return png_reps;
}

// Returns a resampled bitmap of
// |desired_size_in_pixel| x |desired_size_in_pixel| by resampling the best
// bitmap out of |input_bitmaps|. ResizeBitmapByDownsamplingIfPossible() is
// similar to SelectFaviconFrames() but it operates on bitmaps which have
// already been resampled via SelectFaviconFrames().
SkBitmap ResizeBitmapByDownsamplingIfPossible(
    const std::vector<SkBitmap>& input_bitmaps,
    int desired_size_in_pixel) {
  DCHECK(!input_bitmaps.empty());
  DCHECK_NE(desired_size_in_pixel, 0);

  SkBitmap best_bitmap;
  for (size_t i = 0; i < input_bitmaps.size(); ++i) {
    const SkBitmap& input_bitmap = input_bitmaps[i];
    if (input_bitmap.width() == desired_size_in_pixel &&
        input_bitmap.height() == desired_size_in_pixel) {
      return input_bitmap;
    } else if (best_bitmap.isNull()) {
      best_bitmap = input_bitmap;
    } else if (input_bitmap.width() >= best_bitmap.width() &&
               input_bitmap.height() >= best_bitmap.height()) {
      if (best_bitmap.width() < desired_size_in_pixel ||
          best_bitmap.height() < desired_size_in_pixel) {
        best_bitmap = input_bitmap;
      }
    } else {
      if (input_bitmap.width() >= desired_size_in_pixel &&
          input_bitmap.height() >= desired_size_in_pixel) {
        best_bitmap = input_bitmap;
      }
    }
  }

  if (desired_size_in_pixel % best_bitmap.width() == 0 &&
      desired_size_in_pixel % best_bitmap.height() == 0) {
    // Use nearest neighbour resampling if upsampling by an integer. This
    // makes the result look similar to the result of SelectFaviconFrames().
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     desired_size_in_pixel,
                     desired_size_in_pixel);
    bitmap.allocPixels();
    if (!best_bitmap.isOpaque())
      bitmap.eraseARGB(0, 0, 0, 0);

    SkCanvas canvas(bitmap);
    SkRect dest(SkRect::MakeWH(desired_size_in_pixel, desired_size_in_pixel));
    canvas.drawBitmapRect(best_bitmap, NULL, dest);
    return bitmap;
  }
  return skia::ImageOperations::Resize(best_bitmap,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       desired_size_in_pixel,
                                       desired_size_in_pixel);
}

}  // namespace

// static
std::vector<ui::ScaleFactor> FaviconUtil::GetFaviconScaleFactors() {
  const float kScale1x = ui::GetImageScale(ui::SCALE_FACTOR_100P);
  std::vector<ui::ScaleFactor> favicon_scale_factors =
      ui::GetSupportedScaleFactors();

  // The scale factors returned from ui::GetSupportedScaleFactors() are sorted.
  // Insert the 1x scale factor such that GetFaviconScaleFactors() is sorted as
  // well.
  size_t insert_index = favicon_scale_factors.size();
  for (size_t i = 0; i < favicon_scale_factors.size(); ++i) {
    float scale = ui::GetImageScale(favicon_scale_factors[i]);
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
void FaviconUtil::SetFaviconColorSpace(gfx::Image* image) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  image->SetSourceColorSpace(base::mac::GetSystemColorSpace());
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

// static
gfx::Image FaviconUtil::SelectFaviconFramesFromPNGs(
      const std::vector<chrome::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor>& scale_factors,
      int favicon_size) {
  // Create image reps for as many scale factors as possible without resizing
  // the bitmap data or decoding it. FaviconHandler stores already resized
  // favicons into history so no additional resizing should be needed in the
  // common case.
  // Creating the gfx::Image from |png_data| without resizing or decoding if
  // possible is important because:
  // - Sync does a byte-to-byte comparison of gfx::Image::As1xPNGBytes() to
  //   the data it put into the database in order to determine whether any
  //   updates should be pushed to sync.
  // - The decoding occurs on the UI thread and the decoding can be a
  //   significant performance hit if a user has many bookmarks.
  // TODO(pkotwicz): Move the decoding off the UI thread.
  std::vector<gfx::ImagePNGRep> png_reps =
      SelectFaviconFramesFromPNGsWithoutResizing(png_data, scale_factors,
          favicon_size);

  // SelectFaviconFramesFromPNGsWithoutResizing() should have selected the
  // largest favicon if |favicon_size| == 0.
  if (favicon_size == 0)
    return gfx::Image(png_reps);

  std::vector<ui::ScaleFactor> scale_factors_to_generate = scale_factors;
  for (size_t i = 0; i < png_reps.size(); ++i) {
    for (int j = static_cast<int>(scale_factors_to_generate.size()) - 1;
         j >= 0; --j) {
      if (png_reps[i].scale == ui::GetImageScale(scale_factors_to_generate[j]))
        scale_factors_to_generate.erase(scale_factors_to_generate.begin() + j);
    }
  }

  if (scale_factors_to_generate.empty())
    return gfx::Image(png_reps);

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

  gfx::ImageSkia resized_image_skia;
  for (size_t i = 0; i < scale_factors_to_generate.size(); ++i) {
    float scale = ui::GetImageScale(scale_factors_to_generate[i]);
    int desired_size_in_pixel = ceil(favicon_size * scale);
    SkBitmap bitmap = ResizeBitmapByDownsamplingIfPossible(
        bitmaps, desired_size_in_pixel);
    resized_image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  if (png_reps.empty())
    return gfx::Image(resized_image_skia);

  std::vector<gfx::ImageSkiaRep> resized_image_skia_reps =
      resized_image_skia.image_reps();
  for (size_t i = 0; i < resized_image_skia_reps.size(); ++i) {
    scoped_refptr<base::RefCountedBytes> png_bytes(new base::RefCountedBytes());
    if (gfx::PNGCodec::EncodeBGRASkBitmap(
        resized_image_skia_reps[i].sk_bitmap(), false, &png_bytes->data())) {
      png_reps.push_back(gfx::ImagePNGRep(png_bytes,
          resized_image_skia_reps[i].scale()));
    }
  }

  return gfx::Image(png_reps);
}
