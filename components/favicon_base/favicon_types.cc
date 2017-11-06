// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_types.h"

#include "components/favicon_base/fallback_icon_style.h"

namespace favicon_base {

// ---------------------------------------------------------
// FaviconImageResult

FaviconImageResult::FaviconImageResult() {}

FaviconImageResult::~FaviconImageResult() {}

// --------------------------------------------------------
// FaviconRawBitmapResult

FaviconRawBitmapResult::FaviconRawBitmapResult()
    : expired(false), icon_type(IconType::kInvalid) {}

FaviconRawBitmapResult::FaviconRawBitmapResult(
    const FaviconRawBitmapResult& other) = default;

FaviconRawBitmapResult::~FaviconRawBitmapResult() {}

// --------------------------------------------------------
// LargeIconResult

LargeIconResult::LargeIconResult(const FaviconRawBitmapResult& bitmap_in)
    : bitmap(bitmap_in) {}

LargeIconResult::LargeIconResult(FallbackIconStyle* fallback_icon_style_in)
    : fallback_icon_style(fallback_icon_style_in) {}

LargeIconResult::~LargeIconResult() {}

// --------------------------------------------------------
// LargeIconImageResult

LargeIconImageResult::LargeIconImageResult(const gfx::Image& image_in,
                                           const GURL& icon_url_in)
    : image(image_in), icon_url(icon_url_in) {}

LargeIconImageResult::LargeIconImageResult(
    FallbackIconStyle* fallback_icon_style_in)
    : fallback_icon_style(fallback_icon_style_in) {}

LargeIconImageResult::~LargeIconImageResult() {}

int GetUmaFaviconType(IconType icon_type) {
  // These values must stay in sync with the FaviconType enum in
  // histograms/enums.xml.
  switch (icon_type) {
    case IconType::kInvalid:
      break;
    case IconType::kFavicon:
      return 1;
    case IconType::kTouchIcon:
      return 2;
    case IconType::kTouchPrecomposedIcon:
      return 3;
    case IconType::kWebManifestIcon:
      return 4;
  }
  // Unexpected values including multiple bits (accidentally) being set in the
  // input contribute to the zero bucket in release mode.
  NOTREACHED();
  return 0;
}

}  // namespace favicon_base
