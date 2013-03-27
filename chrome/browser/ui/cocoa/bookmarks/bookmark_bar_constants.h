// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for positioning the bookmark bar. These aren't placed in a
// different file because they're conditionally included in cross platform code
// and thus no Objective-C++ stuff.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"

namespace bookmarks {

// Correction used for computing other values based on the height.
const int kVisualHeightOffset = 2;

// Bar height, when opened in "always visible" mode. This is actually a little
// smaller than it should be (by |kVisualHeightOffset| points) because of the
// visual overlap with the main toolbar. When using this to compute values
// other than the actual height of the toolbar, be sure to add
// |kVisualHeightOffset|.
const int kBookmarkBarHeight = 26;

// Height of the bookmark bar on the new tab page when instant extended is
// enabled. TODO(sail): Remove this and update chrome::kNTPBookmarkBarHeight
// instead.
static const int kSearchNewTabBookmarkBarHeight = 40;

// The amount of space between the inner bookmark bar and the outer toolbar on
// new tab pages.
const int kNTPBookmarkBarPadding = (chrome::kNTPBookmarkBarHeight -
    (kBookmarkBarHeight + kVisualHeightOffset)) / 2;

// Same as |kNTPBookmarkBarPadding| except for the instant extended UI.
// TODO(sail): Remove this this and update |kNTPBookmarkBarPadding| instead.
const int kSearchNTPBookmarkBarPadding =
    (kSearchNewTabBookmarkBarHeight -
     (kBookmarkBarHeight + kVisualHeightOffset)) / 2;

// The height of buttons in the bookmark bar.
const int kBookmarkButtonHeight = kBookmarkBarHeight + kVisualHeightOffset;

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
