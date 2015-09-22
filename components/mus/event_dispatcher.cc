// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/event_dispatcher.h"

#include "components/mus/server_view.h"
#include "components/mus/view_coordinate_conversions.h"
#include "components/mus/view_locator.h"
#include "components/mus/view_tree_host_impl.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace mus {

EventDispatcher::EventDispatcher(ViewTreeHostImpl* view_tree_host)
    : view_tree_host_(view_tree_host) {}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::AddAccelerator(uint32_t id,
                                     mojo::KeyboardCode keyboard_code,
                                     mojo::EventFlags flags) {
#if !defined(NDEBUG)
  for (const auto& pair : accelerators_) {
    DCHECK(pair.first != id);
    DCHECK(pair.second.keyboard_code != keyboard_code ||
           pair.second.flags != flags);
  }
#endif
  accelerators_.insert(Entry(id, Accelerator(keyboard_code, flags)));
}

void EventDispatcher::RemoveAccelerator(uint32_t id) {
  auto it = accelerators_.find(id);
  DCHECK(it != accelerators_.end());
  accelerators_.erase(it);
}

void EventDispatcher::OnEvent(mojo::EventPtr event) {
  if (event->action == mojo::EVENT_TYPE_KEY_PRESSED &&
      !event->key_data->is_char) {
    uint32_t accelerator = 0u;
    if (FindAccelerator(*event, &accelerator)) {
      view_tree_host_->OnAccelerator(accelerator, event.Pass());
      return;
    }
  }

  ServerView* target = FindEventTarget(event.get());
  if (target) {
    // Update focus on pointer-down.
    if (event->action == mojo::EVENT_TYPE_POINTER_DOWN)
      view_tree_host_->SetFocusedView(target);
    view_tree_host_->DispatchInputEventToView(target, event.Pass());
  }
}

bool EventDispatcher::FindAccelerator(const mojo::Event& event,
                                      uint32_t* accelerator_id) {
  DCHECK(event.key_data);
  for (const auto& pair : accelerators_) {
    if (pair.second.keyboard_code == event.key_data->windows_key_code &&
        pair.second.flags == event.flags) {
      *accelerator_id = pair.first;
      return true;
    }
  }
  return false;
}

ServerView* EventDispatcher::FindEventTarget(mojo::Event* event) {
  ServerView* focused_view = view_tree_host_->GetFocusedView();
  if (event->pointer_data && event->pointer_data->location) {
    ServerView* root = view_tree_host_->root_view();
    const gfx::Point root_point(
        static_cast<int>(event->pointer_data->location->x),
        static_cast<int>(event->pointer_data->location->y));
    ServerView* target = focused_view;
    if (event->action == mojo::EVENT_TYPE_POINTER_DOWN || !target ||
        !root->Contains(target)) {
      target = FindDeepestVisibleView(root, root_point);
      CHECK(target);
    }
    const gfx::PointF local_point(ConvertPointFBetweenViews(
        root, target, gfx::PointF(event->pointer_data->location->x,
                                  event->pointer_data->location->y)));
    event->pointer_data->location->x = local_point.x();
    event->pointer_data->location->y = local_point.y();
    return target;
  }

  return focused_view;
}

}  // namespace mus
