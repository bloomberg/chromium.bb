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

void EventDispatcher::AddAccelerator(uint32_t id,
                                     mojo::KeyboardCode keyboard_code,
                                     mojo::EventFlags flags) {
#if !defined(NDEBUG)
  std::for_each(accelerators_.begin(), accelerators_.end(),
                [id, keyboard_code, flags](const Entry& entry) {
                  DCHECK(entry.first != id);
                  DCHECK(entry.second.keyboard_code != keyboard_code ||
                         entry.second.flags != flags);
                });
#endif
  accelerators_.insert(Entry(id, Accelerator(keyboard_code, flags)));
}

void EventDispatcher::RemoveAccelerator(uint32_t id) {
  auto it = accelerators_.find(id);
  DCHECK(it != accelerators_.end());
  accelerators_.erase(it);
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
  } else {
    uint32_t accelerator_id = 0;
    if (event->action == mojo::EVENT_TYPE_KEY_PRESSED &&
        HandleAccelerator(event->key_data->windows_key_code, event->flags,
                          &accelerator_id)) {
      // For accelerators, normal event dispatch is bypassed.
      connection_manager_->OnAccelerator(root, accelerator_id, event.Pass());
      return;
    }
    ServerView* focused_view = connection_manager_->GetFocusedView();
    if (focused_view)
      connection_manager_->DispatchInputEventToView(focused_view, event.Pass());
  }
}

bool EventDispatcher::HandleAccelerator(mojo::KeyboardCode keyboard_code,
                                        mojo::EventFlags flags,
                                        uint32_t* accelerator_id) {
  auto it = std::find_if(accelerators_.begin(), accelerators_.end(),
                         [keyboard_code, flags](const Entry& entry) {
                           return entry.second.keyboard_code == keyboard_code &&
                                  entry.second.flags == flags;
                         });
  bool found = it != accelerators_.end();
  if (found)
    *accelerator_id = it->first;
  return found;
}

}  // namespace view_manager
