// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace ash {

WorkspaceEventHandlerAura::WorkspaceEventHandlerAura(WmWindow* workspace_window)
    : workspace_window_(workspace_window) {
  workspace_window_->AddLimitedPreTargetHandler(this);
}

WorkspaceEventHandlerAura::~WorkspaceEventHandlerAura() {
  workspace_window_->RemoveLimitedPreTargetHandler(this);
}

void WorkspaceEventHandlerAura::OnMouseEvent(ui::MouseEvent* event) {
  WorkspaceEventHandler::OnMouseEvent(
      event, WmWindowAura::Get(static_cast<aura::Window*>(event->target())));
}

void WorkspaceEventHandlerAura::OnGestureEvent(ui::GestureEvent* event) {
  WorkspaceEventHandler::OnGestureEvent(
      event, WmWindowAura::Get(static_cast<aura::Window*>(event->target())));
}

}  // namespace ash
