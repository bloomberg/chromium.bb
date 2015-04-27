// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"

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
      desired_size_in_pixel,
      base::Bind(&LargeIconService::RunLargeIconCallback,
                 base::Unretained(this), callback, desired_size_in_pixel),
      tracker);
}

void LargeIconService::RunLargeIconCallback(
    const favicon_base::LargeIconCallback& callback,
    int desired_size_in_pixel,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  // If there are no bitmaps, return a result with an empty |bitmap| and a
  // default |fallback_icon_style|.
  if (!bitmap_result.is_valid()) {
    callback.Run(
        favicon_base::LargeIconResult(new favicon_base::FallbackIconStyle()));
    return;
  }

  // If there is a bitmap but it's smaller than the requested size or
  // non-square, compute its dominant color and use it as background in
  // |fallback_icon_style|.
  if (bitmap_result.pixel_size.width() < desired_size_in_pixel ||
      bitmap_result.pixel_size.height() < desired_size_in_pixel ||
      bitmap_result.pixel_size.width() != bitmap_result.pixel_size.height()) {
    // TODO(beaudoin): Resize the icon if it's large enough. Alternatively,
    // return it and let the HTML resize it.
    favicon_base::LargeIconResult result(new favicon_base::FallbackIconStyle());
    favicon_base::SetDominantColorAsBackground(
        bitmap_result.bitmap_data, result.fallback_icon_style.get());
    callback.Run(result);
    return;
  }

  // The bitmap is square and at least as large as the requested one, return
  // it.
  // TODO(beaudoin): Resize the icon if it's too large. Alternatively, return
  // it and let the HTML resize it.
  callback.Run(favicon_base::LargeIconResult(bitmap_result));
}

}  // namespace favicon
