// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller.h"

#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/laser/laser_pointer_view.h"
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A point gets removed from the collection if it is older than
// |kPointLifeDurationMs|.
const int kPointLifeDurationMs = 200;

// When no move events are being received we add a new point every
// |kAddStationaryPointsDelayMs| so that points older than
// |kPointLifeDurationMs| can get removed.
const int kAddStationaryPointsDelayMs = 5;

aura::Window* GetCurrentRootWindow() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    if (root_window->ContainsPointInRoot(
            root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot()))
      return root_window;
  }
  return nullptr;
}
}  // namespace

LaserPointerController::LaserPointerController()
    : stationary_timer_(new base::Timer(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAddStationaryPointsDelayMs),
          base::Bind(&LaserPointerController::AddStationaryPoint,
                     base::Unretained(this)),
          true /* is_repeating */)) {}

LaserPointerController::~LaserPointerController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void LaserPointerController::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  if (enabled_) {
    Shell::GetInstance()->AddPreTargetHandler(this);
  } else {
    laser_pointer_view_.reset();
    Shell::GetInstance()->RemovePreTargetHandler(this);
  }
}

void LaserPointerController::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_)
    return;

  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_MOUSE_DRAGGED &&
      event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_MOUSE_RELEASED)
    return;

  // Delete the LaserPointerView instance if mouse is released.
  if (event->type() == ui::ET_MOUSE_RELEASED) {
    stationary_timer_->Stop();
    laser_pointer_view_->Stop();
    laser_pointer_view_.reset();
    return;
  }

  // This will handle creating the initial laser pointer view on
  // ET_MOUSE_PRESSED events.
  SwitchTargetRootWindowIfNeeded(GetCurrentRootWindow());

  if (laser_pointer_view_) {
    // Remap point from where it was captured to the display it is actually on.
    gfx::Point event_location = event->root_location();
    aura::Window* target = static_cast<aura::Window*>(event->target());
    aura::Window* event_root = target->GetRootWindow();
    aura::Window::ConvertPointToTarget(
        event_root, laser_pointer_view_->GetRootWindow(), &event_location);

    current_mouse_location_ = event_location;
    laser_pointer_view_->AddNewPoint(current_mouse_location_);

    stationary_timer_repeat_count_ = 0;
    if (event->type() == ui::ET_MOUSE_DRAGGED) {
      // Start the timer to add stationary points if dragged.
      if (!stationary_timer_->IsRunning())
        stationary_timer_->Reset();
    }

    // If the stylus is over the palette icon or widget, do not consume the
    // event.
    if (!PaletteContainsPointInScreen(current_mouse_location_))
      event->StopPropagation();
  }
}

void LaserPointerController::OnWindowDestroying(aura::Window* window) {
  SwitchTargetRootWindowIfNeeded(window);
}

void LaserPointerController::SwitchTargetRootWindowIfNeeded(
    aura::Window* root_window) {
  if (!root_window) {
    stationary_timer_->Stop();
    laser_pointer_view_.reset();
  } else if (laser_pointer_view_) {
    laser_pointer_view_->ReparentWidget(root_window);
  } else if (enabled_) {
    laser_pointer_view_.reset(new LaserPointerView(
        base::TimeDelta::FromMilliseconds(kPointLifeDurationMs), root_window));
  }
}

void LaserPointerController::AddStationaryPoint() {
  laser_pointer_view_->AddNewPoint(current_mouse_location_);
  // We can stop repeating the timer once the mouse has been stationary for
  // longer than the life of a point.
  if (stationary_timer_repeat_count_ * kAddStationaryPointsDelayMs >=
      kPointLifeDurationMs) {
    stationary_timer_->Stop();
  }
  stationary_timer_repeat_count_++;
}
}  // namespace ash
