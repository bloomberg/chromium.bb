// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#pragma once

#include "base/file_path.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
@class NSImage;
@class NSString;
#else  // __OBJC__
class NSImage;
#endif  // __OBJC__

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
                       FilePath::StringType profile_path);

// Reads a set of bookmark elements from the specified pasteboard.
bool ReadFromPasteboard(PasteboardType type,
                        std::vector<BookmarkNodeData::Element>& elements,
                        FilePath* profile_path);

// Returns true if the specified pasteboard contains any sort of
// bookmark elements.  It currently does not consider a plaintext url a
// valid bookmark.
bool PasteboardContainsBookmarks(PasteboardType type);

// Returns a drag image for a bookmark.
NSImage* DragImageForBookmark(NSImage* favicon, const string16& title);

// Copies the bookmark nodes to the dragging pasteboard and initiates a
// drag from the specified view.  |view| must be a |TabContentsViewCocoa*|.
void StartDrag(Profile* profile, const std::vector<const BookmarkNode*>& nodes,
               gfx::NativeView view);

}  // namespace bookmark_pasteboard_helper_mac

#if defined(__OBJC__)
// Pasteboard type for dictionary containing bookmark structure consisting
// of individual bookmark nodes and/or bookmark folders.
extern "C" NSString* const kBookmarkDictionaryListPboardType;
#endif  // __OBJC__

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
