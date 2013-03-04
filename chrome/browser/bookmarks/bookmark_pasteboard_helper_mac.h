// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(__OBJC__)
@class NSString;
#endif  // __OBJC__

namespace base {
class FilePath;
}

// This set of functions lets C++ code interact with the cocoa pasteboard
// and dragging methods.
namespace bookmark_pasteboard_helper_mac {

enum PasteboardType {
  kCopyPastePasteboard,
  kDragPasteboard
};

// Writes a set of bookmark elements from a profile to the specified pasteboard.
void WriteToPasteboard(PasteboardType type,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const base::FilePath& profile_path,
                       ui::Clipboard::SourceTag tag);

// Reads a set of bookmark elements from the specified pasteboard.
bool ReadFromPasteboard(PasteboardType type,
                        std::vector<BookmarkNodeData::Element>& elements,
                        base::FilePath* profile_path);

// Returns true if the specified pasteboard contains any sort of
// bookmark elements.  It currently does not consider a plaintext url a
// valid bookmark.
bool PasteboardContainsBookmarks(PasteboardType type);

}  // namespace bookmark_pasteboard_helper_mac

#if defined(__OBJC__)
// Pasteboard type for dictionary containing bookmark structure consisting
// of individual bookmark nodes and/or bookmark folders.
extern "C" NSString* const kBookmarkDictionaryListPboardType;
#endif  // __OBJC__

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
