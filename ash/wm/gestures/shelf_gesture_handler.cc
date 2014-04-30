// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/shelf_gesture_handler.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/gestures/tray_gesture_handler.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {

ShelfGestureHandler::ShelfGestureHandler()
    : drag_in_progress_(false) {
}

ShelfGestureHandler::~ShelfGestureHandler() {
}

bool ShelfGestureHandler::ProcessGestureEvent(const ui::GestureEvent& event) {
  Shell* shell = Shell::GetInstance();
  if (!shell->session_state_delegate()->NumberOfLoggedInUsers() ||
      shell->session_state_delegate()->IsScreenLocked()) {
    // The gestures are disabled in the lock/login screen.
    return false;
  }

  // TODO(oshima): Find the root window controller from event's location.
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();

  ShelfLayoutManager* shelf = controller->GetShelfLayoutManager();

  if (event.type() == ui::ET_GESTURE_WIN8_EDGE_SWIPE) {
    shelf->OnGestureEdgeSwipe(event);
    return true;
  }

  const aura::Window* fullscreen = controller->GetWindowForFullscreenMode();
  if (fullscreen &&
      ash::wm::GetWindowState(fullscreen)->hide_shelf_when_fullscreen()) {
    return false;
  }

  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    drag_in_progress_ = true;
    shelf->StartGestureDrag(event);
    return true;
  }

  if (!drag_in_progress_)
    return false;

  if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (tray_handler_) {
      if (!tray_handler_->UpdateGestureDrag(event))
        tray_handler_.reset();
    } else if (shelf->UpdateGestureDrag(event) ==
        ShelfLayoutManager::DRAG_TRAY) {
      tray_handler_.reset(new TrayGestureHandler());
    }

    return true;
  }

  drag_in_progress_ = false;

  if (event.type() == ui::ET_GESTURE_SCROLL_END ||
      event.type() == ui::ET_SCROLL_FLING_START) {
    if (tray_handler_) {
      tray_handler_->CompleteGestureDrag(event);
      tray_handler_.reset();
    }

    shelf->CompleteGestureDrag(event);
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  shelf->CancelGestureDrag();
  return false;
}

}  // namespace ash
