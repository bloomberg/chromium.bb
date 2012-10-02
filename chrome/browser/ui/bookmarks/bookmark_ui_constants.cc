// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_ui_constants.h"

#include "build/build_config.h"

namespace chrome {

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
const int kBookmarkBarHeight = 28;
#elif defined(OS_MACOSX)
// This is actually a little smaller than it should be
// (by |kVisualHeightOffset| points) because of the visual overlap with the main
// toolbar. When using this to compute values other than the actual height of
// the toolbar, be sure to add |kVisualHeightOffset|.
const int kBookmarkBarHeight = 26;
#elif defined(TOOLKIT_GTK)
const int kBookmarkBarHeight = 29;
#endif

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
const int kNTPBookmarkBarHeight = 57;
#elif defined(OS_MACOSX)
const int kNTPBookmarkBarHeight = 40;
#elif defined(TOOLKIT_GTK)
const int kNTPBookmarkBarHeight = 57;
#endif

}  // namespace chrome
