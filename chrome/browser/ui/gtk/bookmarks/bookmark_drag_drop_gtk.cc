// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/custom_drag.h"

namespace {

const GdkDragAction kBookmarkDragAction =
    static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE);

// Encapsulates functionality for drags of one or more bookmarks.
class BookmarkDrag : public CustomDrag {
 public:
  BookmarkDrag(Profile* profile, const std::vector<const BookmarkNode*>& nodes);

 private:
  virtual ~BookmarkDrag() {}

  virtual void OnDragDataGet(GtkWidget* widget,
                             GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type,
                             guint time) OVERRIDE {
    WriteBookmarksToSelection(nodes_, selection_data, target_type, profile_);
  }

  Profile* profile_;
  std::vector<const BookmarkNode*> nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDrag);
};

BookmarkDrag::BookmarkDrag(Profile* profile,
                           const std::vector<const BookmarkNode*>& nodes)
    : CustomDrag(NULL, GetCodeMask(false), kBookmarkDragAction),
      profile_(profile),
      nodes_(nodes) {}

}  // namespace

namespace chrome {

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view) {
  DCHECK(!nodes.empty());

  // This starts the drag process, the lifetime of this object is tied to the
  // system drag.
  new BookmarkDrag(profile, nodes);
}

}  // namespace chrome
