// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"

#include "chrome/browser/ui/bookmarks/bookmark_ui_constants.h"

namespace bookmarks {

const int kVisualHeightOffset = 2;

const int kNTPBookmarkBarPadding = (chrome::kNTPBookmarkBarHeight -
    (chrome::kBookmarkBarHeight + kVisualHeightOffset)) / 2;

const int kBookmarkButtonHeight = chrome::kBookmarkBarHeight +
    kVisualHeightOffset;

const CGFloat kBookmarkFolderButtonHeight = 24.0;

const CGFloat kBookmarkBarMenuCornerRadius = 4.0;

}  // namespace bookmarks
