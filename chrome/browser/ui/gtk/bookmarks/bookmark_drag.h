// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_DRAG_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_DRAG_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/custom_drag.h"

class BookmarkNode;
class Profile;

// Encapsulates functionality for drags of one or more bookmarks.
class BookmarkDrag : public CustomDrag {
 public:
  // Creates a new BookmarkDrag, the lifetime of which is tied to the system
  // drag.
  static void BeginDrag(Profile* profile,
                        const std::vector<const BookmarkNode*>& nodes);

 private:
  BookmarkDrag(Profile* profile, const std::vector<const BookmarkNode*>& nodes);
  virtual ~BookmarkDrag();

  virtual void OnDragDataGet(GtkWidget* widget,
                             GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type,
                             guint time) OVERRIDE;

  Profile* profile_;
  std::vector<const BookmarkNode*> nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDrag);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_DRAG_H_
