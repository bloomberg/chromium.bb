// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_

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

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
