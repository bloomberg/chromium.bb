// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/view_targeter.h"

#include "mojo/services/window_manager/capture_controller.h"
#include "mojo/services/window_manager/focus_controller.h"
#include "mojo/services/window_manager/view_target.h"

namespace window_manager {

ViewTargeter::ViewTargeter() {}

ViewTargeter::~ViewTargeter() {}

ui::EventTarget* ViewTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                  ui::Event* event) {
  ViewTarget* view = static_cast<ViewTarget*>(root);
  ViewTarget* target =
      event->IsKeyEvent()
          ? FindTargetForKeyEvent(view, *static_cast<ui::KeyEvent*>(event))
          : static_cast<ViewTarget*>(
                EventTargeter::FindTargetForEvent(root, event));

  // TODO(erg): The aura version of this method does a lot of work to handle
  // dispatching to a target that isn't a child of |view|. For now, punt on
  // this.
  DCHECK_EQ(view->GetRoot(), target->GetRoot());

  return target;
}

ui::EventTarget* ViewTargeter::FindTargetForLocatedEvent(
    ui::EventTarget* root,
    ui::LocatedEvent* event) {
  ViewTarget* view = static_cast<ViewTarget*>(root);
  if (!view->HasParent()) {
    ViewTarget* target = FindTargetInRootView(view, *event);
    if (target) {
      view->ConvertEventToTarget(target, event);
      return target;
    }
  }
  return EventTargeter::FindTargetForLocatedEvent(view, event);
}

bool ViewTargeter::SubtreeCanAcceptEvent(ui::EventTarget* target,
                                         const ui::LocatedEvent& event) const {
  ViewTarget* view = static_cast<ViewTarget*>(target);

  if (!view->IsVisible())
    return false;

  // TODO(erg): We may need to keep track of the parent on ViewTarget, because
  // we have a check here about
  // WindowDelegate::ShouldDescendIntoChildForEventHandling().

  // TODO(sky): decide if we really want this. If we do, it should be a public
  // constant and documented.
  if (view->view()->shared_properties().count("deliver-events-to-parent"))
    return false;

  return true;
}

bool ViewTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  ViewTarget* view = static_cast<ViewTarget*>(target);
  gfx::Point point = event.location();
  const ViewTarget* parent = view->GetParent();
  if (parent)
    ViewTarget::ConvertPointToTarget(parent, view, &point);
  return gfx::Rect(view->GetBounds().size()).Contains(point);
}

ViewTarget* ViewTargeter::FindTargetForKeyEvent(ViewTarget* view_target,
                                                const ui::KeyEvent& key) {
  FocusController* focus_controller = GetFocusController(view_target->view());
  if (focus_controller) {
    mojo::View* focused_view = focus_controller->GetFocusedView();
    if (focused_view)
      return ViewTarget::TargetFromView(focused_view);
  }
  return view_target;
}

ViewTarget* ViewTargeter::FindTargetInRootView(ViewTarget* root_view,
                                               const ui::LocatedEvent& event) {
  // TODO(erg): This here is important because it resolves
  // mouse_pressed_handler() in the aura version. This is what makes sure
  // that a view gets both the mouse down and up.

  CaptureController* capture_controller =
      GetCaptureController(root_view->view());
  if (capture_controller) {
    mojo::View* capture_view = capture_controller->GetCapture();
    if (capture_view)
      return ViewTarget::TargetFromView(capture_view);
  }

  // TODO(erg): There's a whole bunch of junk about handling touch events
  // here. Handle later.

  return nullptr;
}

}  // namespace window_manager
