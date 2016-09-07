// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for positioning the bookmark bar. These aren't placed in a
// different file because they're conditionally included in cross platform code
// and thus no Objective-C++ stuff.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#include <ApplicationServices/ApplicationServices.h>

#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"

namespace bookmarks {

// Correction used for computing other values based on the height.
const int kVisualHeightOffset = 2;
const int kMaterialVisualHeightOffset = 2;

// The amount of space between the inner bookmark bar and the outer toolbar on
// new tab pages.
const int kNTPBookmarkBarPadding =
    (chrome::kNTPBookmarkBarHeight -
     (chrome::kMinimumBookmarkBarHeight + kVisualHeightOffset)) /
    2;

// The height of buttons in the bookmark bar.
const int kBookmarkButtonHeight =
    chrome::kMinimumBookmarkBarHeight + kVisualHeightOffset;

// The height of buttons in a bookmark bar folder menu.
const CGFloat kBookmarkFolderButtonHeight = 24.0;

// The radius of the corner curves on the menu. Also used for sizing the shadow
// window behind the menu window at times when the menu can be scrolled.
const CGFloat kBookmarkBarMenuCornerRadius = 4.0;

// Overlap (in pixels) between the toolbar and the bookmark bar (when showing in
// normal mode).
const CGFloat kBookmarkBarOverlap = 3.0;

}  // namespace bookmarks

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
