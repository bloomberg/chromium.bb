// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_

#include <vector>

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"

class BookmarkNode;
class Profile;

namespace bookmarks {
struct BookmarkNodeData;
}

namespace chrome {

// Starts the process of dragging a folder of bookmarks.
void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view,
                   ui::DragDropTypes::DragEventSource source);

// Drops the bookmark nodes that are in |data| onto |parent_node| at |index|.
// |copy| indicates the source operation: if true then the bookmarks in |data|
// are copied, otherwise they are moved if they belong to the same |profile|.
// Returns the drop type used.
int DropBookmarks(Profile* profile,
                  const bookmarks::BookmarkNodeData& data,
                  const BookmarkNode* parent_node,
                  int index,
                  bool copy);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
