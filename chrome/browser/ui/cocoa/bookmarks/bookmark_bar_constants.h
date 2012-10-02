// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for positioning the bookmark bar. These aren't placed in a
// different file because they're conditionally included in cross platform code
// and thus no Objective-C++ stuff.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#import <Cocoa/Cocoa.h>

namespace bookmarks {

// Correction used for computing other values based on the height.
extern const int kVisualHeightOffset;

// The amount of space between the inner bookmark bar and the outer toolbar on
// new tab pages.
extern const int kNTPBookmarkBarPadding;

// The height of buttons in the bookmark bar.
extern const int kBookmarkButtonHeight;

// The height of buttons in a bookmark bar folder menu.
extern const CGFloat kBookmarkFolderButtonHeight;

// The radius of the corner curves on the menu. Also used for sizing the shadow
// window behind the menu window at times when the menu can be scrolled.
extern const CGFloat kBookmarkBarMenuCornerRadius;

}  // namespace bookmarks

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
