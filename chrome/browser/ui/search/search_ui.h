// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_

#include "third_party/skia/include/core/SkColor.h"

namespace chrome {
namespace search {

// The minimum height of the content view for which the detached bookmark bar
// should be visible. This value is calculated from the
// chrome/browser/resources/ntp_search/ tile_page.js
// HEIGHT_FOR_BOTTOM_PANEL constant.
static const int kMinContentHeightForBottomBookmarkBar = 531;

// The maximum width of the detached bookmark bar.
static const int kMaxWidthForBottomBookmarkBar = 720;

// The left and right padding of the detached bookmark bar.
static const int kHorizontalPaddingForBottomBookmarkBar = 130;

// The alpha used to draw the bookmark bar background when themed.
// This value ranges from 0.0 (fully transparent) to 1.0 (fully opaque).
static const float kBookmarkBarThemeBackgroundAlphaFactor = 0.8f;

// Returns the color to use to draw the bookmark bar separator when not themed.
SkColor GetBookmarkBarNoThemeSeparatorColor();

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
