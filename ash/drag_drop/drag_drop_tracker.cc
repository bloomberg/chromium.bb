// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// Creates a window for capturing drag events.
aura::Window* CreateCaptureWindow(aura::RootWindow* context_root,
                                  aura::WindowDelegate* delegate) {
  aura::Window* window = new aura::Window(delegate);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetDefaultParentByRootWindow(context_root, gfx::Rect());
  window->Show();
  DCHECK(window->bounds().size().IsEmpty());
  return window;
}

}  // namespace

DragDropTracker::DragDropTracker(aura::RootWindow* context_root,
                                 aura::WindowDelegate* delegate)
    : capture_window_(CreateCaptureWindow(context_root, delegate)) {
}

DragDropTracker::~DragDropTracker()  {
  capture_window_->ReleaseCapture();
}

void DragDropTracker::TakeCapture() {
  capture_window_->SetCapture();
}

aura::Window* DragDropTracker::GetTarget(const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point location_in_screen = event.location();
  wm::ConvertPointToScreen(capture_window_.get(),
                           &location_in_screen);
  aura::RootWindow* root_window_at_point =
      wm::GetRootWindowAt(location_in_screen);
  gfx::Point location_in_root = location_in_screen;
  wm::ConvertPointFromScreen(root_window_at_point, &location_in_root);
  return root_window_at_point->GetEventHandlerForPoint(location_in_root);
}

ui::LocatedEvent* DragDropTracker::ConvertEvent(
    aura::Window* target,
    const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point target_location = event.location();
  aura::Window::ConvertPointToTarget(capture_window_.get(), target,
                                     &target_location);
  gfx::Point location_in_screen = event.location();
  ash::wm::ConvertPointToScreen(capture_window_.get(), &location_in_screen);
  gfx::Point target_root_location = event.root_location();
  aura::Window::ConvertPointToTarget(
      capture_window_->GetRootWindow(),
      ash::wm::GetRootWindowAt(location_in_screen),
      &target_root_location);
  return new ui::MouseEvent(event.type(),
                            target_location,
                            target_root_location,
                            event.flags());
}

}  // namespace internal
}  // namespace ash
