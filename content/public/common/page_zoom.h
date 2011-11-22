// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_
#define CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_
#pragma once

#include "content/common/content_export.h"

namespace content {

// This enum is the parameter to various text/page zoom commands so we know
// what the specific zoom command is.
enum PageZoom {
  PAGE_ZOOM_OUT   = -1,
  PAGE_ZOOM_RESET = 0,
  PAGE_ZOOM_IN    = 1,
};

// The minimum zoom factor permitted for a page. This is an alternative to
// WebView::minTextSizeMultiplier.
CONTENT_EXPORT extern const double kMinimumZoomFactor;

// The maximum zoom factor permitted for a page. This is an alternative to
// WebView::maxTextSizeMultiplier.
CONTENT_EXPORT extern const double kMaximumZoomFactor;

// Test if two zoom values (either zoom factors or zoom levels) should be
// considered equal.
CONTENT_EXPORT bool ZoomValuesEqual(double value_a, double value_b);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_
