// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_node_data.h"

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  return bookmark_pasteboard_helper_mac::PasteboardContainsBookmarks(
      bookmark_pasteboard_helper_mac::kCopyPastePasteboard);
}

void BookmarkNodeData::WriteToClipboard() const {
  bookmark_pasteboard_helper_mac::WriteToPasteboard(
      bookmark_pasteboard_helper_mac::kCopyPastePasteboard,
      elements,
      profile_path_);
}

bool BookmarkNodeData::ReadFromClipboard() {
  base::FilePath file_path;
  if (!bookmark_pasteboard_helper_mac::ReadFromPasteboard(
          bookmark_pasteboard_helper_mac::kCopyPastePasteboard,
          elements,
          &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}

bool BookmarkNodeData::ReadFromDragClipboard() {
  base::FilePath file_path;
  if (!bookmark_pasteboard_helper_mac::ReadFromPasteboard(
          bookmark_pasteboard_helper_mac::kDragPasteboard,
          elements,
          &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}
