// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_node_data.h"

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  return PasteboardContainsBookmarks(BOOKMARK_PASTEBOARD_TYPE_COPY_PASTE);
}

void BookmarkNodeData::WriteToClipboard() const {
  WriteBookmarksToPasteboard(
      BOOKMARK_PASTEBOARD_TYPE_COPY_PASTE, elements, profile_path_);
}

bool BookmarkNodeData::ReadFromClipboard() {
  base::FilePath file_path;
  if (!ReadBookmarksFromPasteboard(
          BOOKMARK_PASTEBOARD_TYPE_COPY_PASTE, elements, &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}

bool BookmarkNodeData::ReadFromDragClipboard() {
  base::FilePath file_path;
  if (!ReadBookmarksFromPasteboard(
          BOOKMARK_PASTEBOARD_TYPE_DRAG, elements, &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}
