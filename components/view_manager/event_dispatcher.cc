// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/event_dispatcher.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/view_coordinate_conversions.h"
#include "components/view_manager/view_locator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace view_manager {

EventDispatcher::EventDispatcher(ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
}

EventDispatcher::~EventDispatcher() {
}

void EventDispatcher::AddAccelerator(mojo::KeyboardCode keyboard_code,
                                     mojo::EventFlags flags) {
  accelerators_.insert(Accelerator(keyboard_code, flags));
}

void EventDispatcher::RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                                        mojo::EventFlags flags) {
  accelerators_.erase(Accelerator(keyboard_code, flags));
}

void EventDispatcher::OnEvent(ServerView* root, mojo::EventPtr event) {
  if (event->pointer_data) {
    const gfx::Point root_point(static_cast<int>(event->pointer_data->x),
                                static_cast<int>(event->pointer_data->y));
    ServerView* target = connection_manager_->GetFocusedView();
    if (event->action == mojo::EVENT_TYPE_POINTER_DOWN || !target ||
        !root->Contains(target)) {
      target = FindDeepestVisibleView(root, root_point);
      CHECK(target);
      connection_manager_->SetFocusedView(target);
    }
    const gfx::PointF local_point(ConvertPointFBetweenViews(
        root, target,
        gfx::PointF(event->pointer_data->x, event->pointer_data->y)));
    event->pointer_data->x = local_point.x();
    event->pointer_data->y = local_point.y();
    connection_manager_->DispatchInputEventToView(target, event.Pass());
  } else if (event->action == mojo::EVENT_TYPE_KEY_PRESSED &&
             accelerators_.count(Accelerator(event->key_data->windows_key_code,
                                             event->flags))) {
    connection_manager_->OnAccelerator(root, event.Pass());
  } else {
    ServerView* focused_view = connection_manager_->GetFocusedView();
    if (focused_view)
      connection_manager_->DispatchInputEventToView(focused_view, event.Pass());
  }
}

}  // namespace view_manager
