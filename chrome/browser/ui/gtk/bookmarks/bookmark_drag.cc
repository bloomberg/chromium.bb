// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_drag.h"

#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"

namespace {

const GdkDragAction kBookmarkDragAction =
    static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE);

}  // namespace

// static
void BookmarkDrag::BeginDrag(Profile* profile,
                             const std::vector<const BookmarkNode*>& nodes) {
  new BookmarkDrag(profile, nodes);
}

BookmarkDrag::BookmarkDrag(Profile* profile,
                           const std::vector<const BookmarkNode*>& nodes)
    : CustomDrag(NULL, GetCodeMask(false), kBookmarkDragAction),
      profile_(profile),
      nodes_(nodes) {}

BookmarkDrag::~BookmarkDrag() {}

void BookmarkDrag::OnDragDataGet(GtkWidget* widget,
                                 GdkDragContext* context,
                                 GtkSelectionData* selection_data,
                                 guint target_type,
                                 guint time) {
  WriteBookmarksToSelection(nodes_, selection_data, target_type, profile_);
}
