// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler_aura.h"

#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace ash {

WorkspaceEventHandlerAura::WorkspaceEventHandlerAura(
    aura::Window* workspace_window)
    : workspace_window_(workspace_window) {
  wm::AddLimitedPreTargetHandlerForWindow(this, workspace_window_);
}

WorkspaceEventHandlerAura::~WorkspaceEventHandlerAura() {
  wm::RemoveLimitedPreTargetHandlerForWindow(this, workspace_window_);
}

void WorkspaceEventHandlerAura::OnMouseEvent(ui::MouseEvent* event) {
  WorkspaceEventHandler::OnMouseEvent(
      event, static_cast<aura::Window*>(event->target()));
}

void WorkspaceEventHandlerAura::OnGestureEvent(ui::GestureEvent* event) {
  WorkspaceEventHandler::OnGestureEvent(
      event, static_cast<aura::Window*>(event->target()));
}

}  // namespace ash
