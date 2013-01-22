// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {
static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";
}

namespace thumbnails {

SimpleThumbnailCrop::SimpleThumbnailCrop(const gfx::Size& target_size)
    : target_size_(target_size) {
  DCHECK(!target_size.IsEmpty());
}

ClipResult SimpleThumbnailCrop::GetCanvasCopyInfo(
    const gfx::Size& source_size,
    ui::ScaleFactor scale_factor,
    gfx::Rect* clipping_rect,
    gfx::Size* target_size) const {
  DCHECK(!source_size.IsEmpty());
  ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  *clipping_rect = GetClippingRect(source_size, target_size_, &clip_result);
  *target_size = GetCopySizeForThumbnail(scale_factor, target_size_);
  return clip_result;
}

void SimpleThumbnailCrop::ProcessBitmap(ThumbnailingContext* context,
                                        const ConsumerCallback& callback,
                                        const SkBitmap& bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (bitmap.isNull() || bitmap.empty())
    return;

  SkBitmap thumbnail = CreateThumbnail(bitmap,
                                       GetThumbnailSizeInPixel(),
                                       &context->clip_result);

  context->score.boring_score = CalculateBoringScore(thumbnail);
  context->score.good_clipping =
      (context->clip_result == CLIP_RESULT_WIDER_THAN_TALL ||
       context->clip_result == CLIP_RESULT_TALLER_THAN_WIDE ||
       context->clip_result == CLIP_RESULT_NOT_CLIPPED);

  callback.Run(*context, thumbnail);
}

double SimpleThumbnailCrop::CalculateBoringScore(const SkBitmap& bitmap) {
  if (bitmap.isNull() || bitmap.empty())
    return 1.0;
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap.width() * bitmap.height();
  return static_cast<double>(color_count) / pixel_count;
}

SkBitmap SimpleThumbnailCrop::GetClippedBitmap(const SkBitmap& bitmap,
                                               int desired_width,
                                               int desired_height,
                                               ClipResult* clip_result) {
  gfx::Rect clipping_rect =
      GetClippingRect(gfx::Size(bitmap.width(), bitmap.height()),
                      gfx::Size(desired_width, desired_height),
                      clip_result);
  SkIRect src_rect = { clipping_rect.x(), clipping_rect.y(),
                       clipping_rect.right(), clipping_rect.bottom() };
  SkBitmap clipped_bitmap;
  bitmap.extractSubset(&clipped_bitmap, src_rect);
  return clipped_bitmap;
}

// Returns the size used by RenderWidgetHost::CopyFromBackingStore.
//
// The size is calculated in such a way that the copied size in pixel becomes
// equal to (f * kThumbnailWidth, f * kThumbnailHeight), where f is the scale
// of ui::SCALE_FACTOR_200P. Since RenderWidgetHost::CopyFromBackingStore takes
// the size in DIP, we need to adjust the size based on |view|'s device scale
// factor in order to copy the pixels with the size above.
//
// The copied size was chosen for the following reasons.
//
// 1. When the scale factor of the primary monitor is ui::SCALE_FACTOR_200P, the
// generated thumbnail size is (f * kThumbnailWidth, f * kThumbnailHeight).
// In order to avoid degrading the image quality by magnification, the size
// of the copied pixels should be equal to or larger than this thumbnail size.
//
// 2. RenderWidgetHost::CopyFromBackingStore can be costly especially when
// it is necessary to read back the web contents image data from GPU. As the
// cost is roughly propotional to the number of the copied pixels, the size of
// the copied pixels should be as small as possible.
//
// When the scale factor of the primary monitor is ui::SCALE_FACTOR_100P,
// we still copy the pixels with the same size as ui::SCALE_FACTOR_200P because
// the resampling method used in RenderWidgetHost::CopyFromBackingStore is not
// good enough for the resampled image to be used directly for the thumbnail
// (http://crbug.com/141235). We assume this is not an issue in case of
// ui::SCALE_FACTOR_200P because the high resolution thumbnail on high density
// display alleviates the aliasing.
// TODO(mazda): Copy the pixels with the smaller size in the case of
// ui::SCALE_FACTOR_100P once the resampling method has been improved.
// static
gfx::Size SimpleThumbnailCrop::GetCopySizeForThumbnail(
    ui::ScaleFactor scale_factor,
    const gfx::Size& thumbnail_size) {
  gfx::Size copy_size(thumbnail_size);
  switch (scale_factor) {
    case ui::SCALE_FACTOR_100P:
      copy_size = gfx::ToFlooredSize(gfx::ScaleSize(
          copy_size, ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P)));
      break;
    case ui::SCALE_FACTOR_200P:
      // Use the size as-is.
      break;
    default:
      DLOG(WARNING) << "Unsupported scale factor. Use the same copy size as "
                    << "ui::SCALE_FACTOR_100P";
      copy_size = gfx::ToFlooredSize(gfx::ScaleSize(
          copy_size, ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P)));
      break;
  }
  return copy_size;
}

SimpleThumbnailCrop::~SimpleThumbnailCrop() {
}

// Returns the size of the thumbnail stored in the database in pixel.
gfx::Size SimpleThumbnailCrop::GetThumbnailSizeInPixel() const {
  // Determine the resolution of the thumbnail based on the maximum scale
  // factor.
  // TODO(mazda|oshima): Update thumbnail when the max scale factor changes.
  // crbug.com/159157.
  float max_scale_factor =
      ui::GetScaleFactorScale(ui::GetMaxScaleFactor());
  return gfx::ToFlooredSize(gfx::ScaleSize(target_size_, max_scale_factor));
}

gfx::Rect SimpleThumbnailCrop::GetClippingRect(const gfx::Size& source_size,
                                               const gfx::Size& desired_size,
                                               ClipResult* clip_result) {
  DCHECK(clip_result);

  float desired_aspect =
      static_cast<float>(desired_size.width()) / desired_size.height();

  // Get the clipping rect so that we can preserve the aspect ratio while
  // filling the destination.
  gfx::Rect clipping_rect;
  if (source_size.width() < desired_size.width() ||
      source_size.height() < desired_size.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    clipping_rect = gfx::Rect(desired_size);
    *clip_result = thumbnails::CLIP_RESULT_SOURCE_IS_SMALLER;
  } else {
    float src_aspect =
        static_cast<float>(source_size.width()) / source_size.height();
    if (src_aspect > desired_aspect) {
      // Wider than tall, clip horizontally: we center the smaller
      // thumbnail in the wider screen.
      int new_width = static_cast<int>(source_size.height() * desired_aspect);
      int x_offset = (source_size.width() - new_width) / 2;
      clipping_rect.SetRect(x_offset, 0, new_width, source_size.height());
      *clip_result = (src_aspect >= ThumbnailScore::kTooWideAspectRatio) ?
          thumbnails::CLIP_RESULT_MUCH_WIDER_THAN_TALL :
          thumbnails::CLIP_RESULT_WIDER_THAN_TALL;
    } else if (src_aspect < desired_aspect) {
      clipping_rect =
          gfx::Rect(source_size.width(), source_size.width() / desired_aspect);
      *clip_result = thumbnails::CLIP_RESULT_TALLER_THAN_WIDE;
    } else {
      clipping_rect = gfx::Rect(source_size);
      *clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
    }
  }
  return clipping_rect;
}

// Creates a downsampled thumbnail from the given bitmap.
// store. The returned bitmap will be isNull if there was an error creating it.
SkBitmap SimpleThumbnailCrop::CreateThumbnail(const SkBitmap& bitmap,
                                              const gfx::Size& desired_size,
                                              ClipResult* clip_result) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  SkBitmap clipped_bitmap;
  if (*clip_result == thumbnails::CLIP_RESULT_UNPROCESSED) {
    // Clip the pixels that will commonly hold a scrollbar, which looks bad in
    // thumbnails.
    int scrollbar_size = gfx::scrollbar_size();
    SkIRect scrollbarless_rect =
        { 0, 0,
          std::max(1, bitmap.width() - scrollbar_size),
          std::max(1, bitmap.height() - scrollbar_size) };
    SkBitmap bmp;
    bitmap.extractSubset(&bmp, scrollbarless_rect);

    clipped_bitmap = GetClippedBitmap(
        bmp, desired_size.width(), desired_size.height(), clip_result);
  } else {
    clipped_bitmap = bitmap;
  }

  // Need to resize it to the size we want, so downsample until it's
  // close, and let the caller make it the exact size if desired.
  SkBitmap result = SkBitmapOperations::DownsampleByTwoUntilSize(
      clipped_bitmap, desired_size.width(), desired_size.height());
#if !defined(USE_AURA)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic
  // ones in PlatformCanvas can't be assigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler
  // will return the input bitmap, which will be the reference to the
  // weird PlatformCanvas one insetad of a regular one. To get a
  // regular refcounted bitmap, we need to copy it.
  //
  // On Aura, the PlatformCanvas is platform-independent and does not have
  // any native platform resources that can't be refounted, so this issue does
  // not occur.
  //
  // Note that GetClippedBitmap() does extractSubset() but it won't copy
  // the pixels, hence we check result size == clipped_bitmap size here.
  if (clipped_bitmap.width() == result.width() &&
      clipped_bitmap.height() == result.height())
    clipped_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
#endif

  HISTOGRAM_TIMES(kThumbnailHistogramName,
                  base::TimeTicks::Now() - begin_compute_thumbnail);
  return result;
}

}
