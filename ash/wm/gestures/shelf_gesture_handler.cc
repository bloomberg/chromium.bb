// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/shelf_gesture_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shelf_types.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/gestures/tray_gesture_handler.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace {

// A ShelfResetHandler auto-hides the shelf as soon as the user interacts with
// any non-shelf part of the system. The ShelfResetHandler manages its own
// lifetime.
class ShelfResetHandler : public ui::EventHandler,
                          public ash::internal::ShelfLayoutManager::Observer {
 public:
  explicit ShelfResetHandler(ash::internal::ShelfLayoutManager* shelf)
      : shelf_(shelf) {
    shelf_->AddObserver(this);
    ash::Shell::GetInstance()->AddPreTargetHandler(this);
  }

 private:
  virtual ~ShelfResetHandler() {
    ash::Shell::GetInstance()->RemovePreTargetHandler(this);
    shelf_->RemoveObserver(this);
  }

  void ResetShelfState() {
    shelf_->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
    delete this;
  }

  void DecideShelfVisibility(const gfx::Point& location) {
    // For the rest of the mouse events, ignore if the event happens inside the
    // shelf.
    views::Widget* widget = shelf_->launcher_widget();
    if (widget &&
        widget->GetWindowBoundsInScreen().Contains(location)) {
      return;
    }

    widget = shelf_->status_area_widget();
    if (widget &&
        widget->GetWindowBoundsInScreen().Contains(location)) {
      return;
    }

    ResetShelfState();
  }

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    ResetShelfState();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    // Ignore all mouse move events.
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_MOUSE_RELEASED) {
      DecideShelfVisibility(event->root_location());
    }
  }

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    DecideShelfVisibility(event->root_location());
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    DecideShelfVisibility(event->root_location());
  }

  // Overridden from ash::internal::ShelfLayoutManager::Observer:
  virtual void WillDeleteShelf() OVERRIDE {
    delete this;
  }

  ash::internal::ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfResetHandler);
};

}  // namespace

namespace ash {
namespace internal {

ShelfGestureHandler::ShelfGestureHandler()
    : drag_in_progress_(false) {
}

ShelfGestureHandler::~ShelfGestureHandler() {
}

bool ShelfGestureHandler::ProcessGestureEvent(const ui::GestureEvent& event) {
  Shell* shell = Shell::GetInstance();
  if (!shell->delegate()->IsUserLoggedIn() ||
      shell->delegate()->IsScreenLocked()) {
    // The gestures are disabled in the lock/login screen.
    return false;
  }

  // The gesture are disabled for fullscreen windows.
  aura::Window* active = wm::GetActiveWindow();
  if (active && wm::IsWindowFullscreen(active))
    return false;

  // TODO(oshima): Find the root window controller from event's location.
  ShelfLayoutManager* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    drag_in_progress_ = true;
    shelf->StartGestureDrag(event);
    return true;
  }

  if (!drag_in_progress_)
    return false;

  if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (tray_handler_.get()) {
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
    if (tray_handler_.get()) {
      tray_handler_->CompleteGestureDrag(event);
      tray_handler_.reset();
    }

    ShelfVisibilityState old_state = shelf->visibility_state();
    shelf->CompleteGestureDrag(event);
    ShelfVisibilityState new_state = shelf->visibility_state();
    if (new_state != old_state && new_state == SHELF_VISIBLE)
      new ShelfResetHandler(shelf);
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  shelf->CancelGestureDrag();
  return false;
}

}  // namespace internal
}  // namespace ash
