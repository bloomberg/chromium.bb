// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#include "build/build_config.h"

namespace chrome {

// The height of Bookmarks Bar, when visible in "New Tab Page" mode.
const int kNTPBookmarkBarHeight = 40;

// The minimum height of Bookmarks Bar, when attached to the toolbar. The
// height of the toolbar may grow to more than this value if the embedded
// views need more space, for example, when the font is larger than normal.
#if defined(OS_MACOSX)
// This is actually a little smaller than it should be (by |kVisualHeightOffset|
// points) because of the visual overlap with the main toolbar. When using this
// to compute values other than the actual height of the toolbar, be sure to add
// |kVisualHeightOffset|.
const int kMinimumBookmarkBarHeight = 26;
#elif defined(TOOLKIT_VIEWS)
const int kMinimumBookmarkBarHeight = 28;
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
