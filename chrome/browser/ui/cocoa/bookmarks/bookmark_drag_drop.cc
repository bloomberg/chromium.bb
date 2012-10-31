// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"

namespace bookmark_utils {

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view) {
  DCHECK(!nodes.empty());

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = MessageLoop::current()->IsNested();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  bookmark_pasteboard_helper_mac::StartDrag(profile, nodes, view);
  MessageLoop::current()->SetNestableTasksAllowed(was_nested);
}

}  // namespace bookmark_utils
