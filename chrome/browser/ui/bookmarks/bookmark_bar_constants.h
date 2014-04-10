// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#include "build/build_config.h"

namespace chrome {

// The height of Bookmarks Bar, when visible in "New Tab Page" mode.
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
const int kNTPBookmarkBarHeight = 40;
#endif

// The height of Bookmarks Bar, when attached to the toolbar.
#if defined(TOOLKIT_VIEWS)
const int kBookmarkBarHeight = 28;
#elif defined(OS_MACOSX)
// This is actually a little smaller than it should be (by |kVisualHeightOffset|
// points) because of the visual overlap with the main toolbar. When using this
// to compute values other than the actual height of the toolbar, be sure to add
// |kVisualHeightOffset|.
const int kBookmarkBarHeight = 26;
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
