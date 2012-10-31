// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "chrome/browser/ui/gtk/custom_drag.h"

namespace bookmark_utils {

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view) {
  DCHECK(!nodes.empty());

  BookmarkDrag::BeginDrag(profile, nodes);
}

}  // namespace bookmark_utils
