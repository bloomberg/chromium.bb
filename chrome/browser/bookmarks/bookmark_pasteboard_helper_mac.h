// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
#pragma once

#include "base/file_path.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "ui/gfx/native_widget_types.h"

// This set of functions lets C++ code interact with the cocoa pasteboard
// and dragging methods.
namespace bookmark_pasteboard_helper_mac {

// Writes a set of bookmark elements from a profile to the general pasteboard.
// This should be used for copy/paste functions.
void WriteToClipboard(const std::vector<BookmarkNodeData::Element>& elements,
                      FilePath::StringType profile_path);

// Writes a set of bookmark elements from a profile to the dragging pasteboard
// for drag and drop functions.
void WriteToDragClipboard(
    const std::vector<BookmarkNodeData::Element>& elements,
    FilePath::StringType profile_path);

// Reads a set of bookmark elements from the general copy/paste clipboard.
bool ReadFromClipboard(std::vector<BookmarkNodeData::Element>& elements,
                       FilePath::StringType* profile_path);

// Reads a set of bookmark elements from the drag and drop clipboard.
bool ReadFromDragClipboard(std::vector<BookmarkNodeData::Element>& elements,
                           FilePath::StringType* profile_path);

// Returns true if the general copy/paste pasteboard contains any sort of
// bookmark elements.  It currently does not consider a plaintext url a
// valid bookmark.
bool ClipboardContainsBookmarks();

// Returns true if the dragging pasteboard contains any sort of bookmark
// elements.
bool DragClipboardContainsBookmarks();

// Copies the bookmark nodes to the dragging pasteboard and initiates a
// drag from the specified view.  |view| must be a |TabContentsViewCocoa*|.
void StartDrag(Profile* profile, const std::vector<const BookmarkNode*>& nodes,
               gfx::NativeView view);

}

#ifdef __OBJC__
@class NSString;
// Pasteboard type for dictionary containing bookmark structure consisting
// of individual bookmark nodes and/or bookmark folders.
extern "C" NSString* const kBookmarkDictionaryListPboardType;
#endif  // __OBJC__

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PASTEBOARD_HELPER_MAC_H_
