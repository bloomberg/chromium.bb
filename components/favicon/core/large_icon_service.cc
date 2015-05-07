// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace favicon {

LargeIconService::LargeIconService(FaviconService* favicon_service)
    : favicon_service_(favicon_service) {
  large_icon_types_.push_back(favicon_base::IconType::FAVICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_ICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_PRECOMPOSED_ICON);
}

LargeIconService::~LargeIconService() {
}

base::CancelableTaskTracker::TaskId
    LargeIconService::GetLargeIconOrFallbackStyle(
        const GURL& page_url,
        int min_source_size_in_pixel,
        int desired_size_in_pixel,
        const favicon_base::LargeIconCallback& callback,
        base::CancelableTaskTracker* tracker) {
  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the best
  //   possible large icon. Also add logic to fetch-on-demand when the URL of
  //   a large icon is known but its bitmap is not available.
  return favicon_service_->GetLargestRawFaviconForPageURL(
      page_url,
      large_icon_types_,
      std::max(min_source_size_in_pixel, desired_size_in_pixel),
      base::Bind(&LargeIconService::RunLargeIconCallback,
                 base::Unretained(this), callback, min_source_size_in_pixel,
                 desired_size_in_pixel),
      tracker);
}

bool LargeIconService::ResizeLargeIconIfValid(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    favicon_base::FaviconRawBitmapResult* resized_bitmap_result) {
  // Require bitmap to be valid and square.
  if (!bitmap_result.is_valid() ||
      bitmap_result.pixel_size.width() != bitmap_result.pixel_size.height())
    return false;

  // Require bitmap to be large enough. It's square, so just check width.
  if (bitmap_result.pixel_size.width() < min_source_size_in_pixel)
    return false;

  *resized_bitmap_result = bitmap_result;

  // Special case: Can use |bitmap_result| as is.
  if (desired_size_in_pixel == 0 ||
      bitmap_result.pixel_size.width() == desired_size_in_pixel)
    return true;

  // Resize bitmap: decode PNG, resize, and re-encode PNG.
  SkBitmap decoded_bitmap;
  if (!gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
          bitmap_result.bitmap_data->size(), &decoded_bitmap))
    return false;

  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      decoded_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      desired_size_in_pixel, desired_size_in_pixel);

  std::vector<unsigned char> bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(resized_bitmap, false, &bitmap_data))
    return false;

  resized_bitmap_result->pixel_size =
      gfx::Size(desired_size_in_pixel, desired_size_in_pixel);
  resized_bitmap_result->bitmap_data =
      base::RefCountedBytes::TakeVector(&bitmap_data);
  return true;
}

void LargeIconService::RunLargeIconCallback(
    const favicon_base::LargeIconCallback& callback,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  favicon_base::FaviconRawBitmapResult resized_bitmap_result;
  if (ResizeLargeIconIfValid(min_source_size_in_pixel, desired_size_in_pixel,
                             bitmap_result, &resized_bitmap_result)) {
    callback.Run(favicon_base::LargeIconResult(resized_bitmap_result));
    return;
  }

  // Failed to resize |bitmap_result|, so compute fallback icon style.
  favicon_base::LargeIconResult result(new favicon_base::FallbackIconStyle());
  if (bitmap_result.is_valid()) {
    favicon_base::SetDominantColorAsBackground(
        bitmap_result.bitmap_data, result.fallback_icon_style.get());
  }
  callback.Run(result);
}

}  // namespace favicon
