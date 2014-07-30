// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_DRAG_DROP_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_DRAG_DROP_VIEWS_H_

class BookmarkNode;
class Profile;

namespace bookmarks {
struct BookmarkNodeData;
}

namespace content {
class BrowserContext;
}

namespace ui {
class DropTargetEvent;
}

namespace chrome {

// Returns the drag operations for the specified node.
int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const BookmarkNode* node);

// Calculates the drop operation given |source_operations| and the ideal
// set of drop operations (|operations|). This prefers the following ordering:
// COPY, LINK then MOVE.
int GetPreferredBookmarkDropOperation(int source_operations, int operations);

// Returns the preferred drop operation on a bookmark menu/bar.
// |parent| is the parent node the drop is to occur on and |index| the index the
// drop is over.
int GetBookmarkDropOperation(Profile* profile,
                             const ui::DropTargetEvent& event,
                             const bookmarks::BookmarkNodeData& data,
                             const BookmarkNode* parent,
                             int index);

// Returns true if the bookmark data can be dropped on |drop_parent| at
// |index|. A drop from a separate profile is always allowed, where as
// a drop from the same profile is only allowed if none of the nodes in
// |data| are an ancestor of |drop_parent| and one of the nodes isn't already
// a child of |drop_parent| at |index|.
bool IsValidBookmarkDropLocation(Profile* profile,
                                 const bookmarks::BookmarkNodeData& data,
                                 const BookmarkNode* drop_parent,
                                 int index);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_DRAG_DROP_VIEWS_H_
