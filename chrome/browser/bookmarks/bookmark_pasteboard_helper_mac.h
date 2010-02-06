// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_

#import "chrome/browser/bookmarks/bookmark_drag_data.h"

namespace bookmark_pasteboard_helper_mac {

void WriteToClipboard(const std::vector<BookmarkDragData::Element>& elements);

bool ReadFromClipboard(std::vector<BookmarkDragData::Element>& elements);

bool ClipboardContainsBookmarks();

}

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
