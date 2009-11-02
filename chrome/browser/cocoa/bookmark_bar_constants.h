// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for positioning the bookmark bar. These aren't placed in a
// different file because they're conditionally included in cross platform code
// and thus no Objective-C++ stuff.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONSTANTS_H_

namespace bookmarks {

// Our height, when opened in "always visible" mode.
const int kBookmarkBarHeight = 28;

// Our height, when visible in "new tab page" mode.
const int kNTPBookmarkBarHeight = 40;

// The amount of space between the inner bookmark bar and the outer toolbar on
// new tab pages.
const int kNTPBookmarkBarPadding =
    (kNTPBookmarkBarHeight - kBookmarkBarHeight) / 2;

// The height of buttons in the bookmark bar.
const int kBookmarkButtonHeight = kBookmarkBarHeight;

}  // namespace bookmarks

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONSTANTS_H_
