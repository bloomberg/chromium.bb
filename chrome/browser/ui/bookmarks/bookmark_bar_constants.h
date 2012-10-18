// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_

#include "build/build_config.h"

namespace chrome {

// The Bookmark bar height, when visible in "new tab page" mode.
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS) || defined(TOOLKIT_GTK)
const int kNTPBookmarkBarHeight = 57;
#elif defined(OS_MACOSX)
const int kNTPBookmarkBarHeight = 40;
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONSTANTS_H_
