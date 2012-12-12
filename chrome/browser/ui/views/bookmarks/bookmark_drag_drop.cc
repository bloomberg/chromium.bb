// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "base/message_loop.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events/event.h"
#include "ui/views/drag_utils.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

namespace bookmark_utils {

void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view) {
  DCHECK(!nodes.empty());

  // Set up our OLE machinery
  ui::OSExchangeData data;
  BookmarkNodeData drag_data(nodes);
  drag_data.Write(profile, &data);

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = MessageLoop::current()->IsNested();
  MessageLoop::current()->SetNestableTasksAllowed(true);

  int operation = ui::DragDropTypes::DRAG_COPY |
                  ui::DragDropTypes::DRAG_MOVE |
                  ui::DragDropTypes::DRAG_LINK;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(view);
  // TODO(varunjain): Properly determine and send DRAG_EVENT_SOURCE below.
  if (widget) {
    widget->RunShellDrag(NULL, data, gfx::Point(), operation,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  } else {
    // We hit this case when we're using WebContentsViewWin or
    // WebContentsViewAura, instead of WebContentsViewViews.
    views::RunShellDrag(view, data, gfx::Point(), operation,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  MessageLoop::current()->SetNestableTasksAllowed(was_nested);
}

}  // namespace bookmark_utils
