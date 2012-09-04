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

namespace {

// Creates a window for capturing drag events.
aura::Window* CreateCaptureWindow(aura::RootWindow* root_window) {
  aura::Window* window = new aura::Window(NULL);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetParent(NULL);
  window->SetBoundsInScreen(root_window->GetBoundsInScreen(),
                            gfx::Screen::GetDisplayNearestWindow(root_window));
  window->Show();
  return window;
}

}  // namespace

namespace ash {
namespace internal {

DragDropTracker::DragDropTracker(aura::RootWindow* root_window)
    : capture_window_(CreateCaptureWindow(root_window)) {
  capture_window_->SetCapture();
}

DragDropTracker::~DragDropTracker()  {
  capture_window_->ReleaseCapture();
}

aura::Window* DragDropTracker::GetTarget(const ui::LocatedEvent& event) {
  std::pair<aura::RootWindow*, gfx::Point> pair =
      ash::wm::GetRootWindowRelativeToWindow(capture_window_.get(),
                                             event.location());
  return pair.first->GetEventHandlerForPoint(pair.second);
}

ui::MouseEvent* DragDropTracker::ConvertMouseEvent(
    aura::Window* target,
    const ui::MouseEvent& event) {
  DCHECK(capture_window_.get());
  std::pair<aura::RootWindow*, gfx::Point> location_pair =
      ash::wm::GetRootWindowRelativeToWindow(capture_window_.get(),
                                             event.location());
  aura::Window::ConvertPointToTarget(location_pair.first, target,
                                     &location_pair.second);
  std::pair<aura::RootWindow*, gfx::Point> root_location_pair =
      ash::wm::GetRootWindowRelativeToWindow(capture_window_->GetRootWindow(),
                                             event.root_location());
  return new ui::MouseEvent(event.type(),
                            location_pair.second,
                            root_location_pair.second,
                            event.flags());
}

}  // namespace internal
}  // namespace ash
