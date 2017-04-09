// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_context_ash.h"

#include "ash/shared/immersive_fullscreen_controller.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm_shell.h"
#include "ash/wm_window.h"
#include "base/logging.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

ImmersiveContextAsh::ImmersiveContextAsh() {}

ImmersiveContextAsh::~ImmersiveContextAsh() {}

void ImmersiveContextAsh::InstallResizeHandleWindowTargeter(
    ImmersiveFullscreenController* controller) {
  WmWindow* window = WmWindow::Get(controller->widget()->GetNativeWindow());
  window->InstallResizeHandleWindowTargeter(controller);
}

void ImmersiveContextAsh::OnEnteringOrExitingImmersive(
    ImmersiveFullscreenController* controller,
    bool entering) {
  WmWindow* window = WmWindow::Get(controller->widget()->GetNativeWindow());
  wm::WindowState* window_state = window->GetWindowState();
  // Auto hide the shelf in immersive fullscreen instead of hiding it.
  window_state->set_hide_shelf_when_fullscreen(!entering);
  // Update the window's immersive mode state for the window manager.
  window_state->set_in_immersive_fullscreen(entering);

  for (WmWindow* root_window : WmShell::Get()->GetAllRootWindows())
    WmShelf::ForWindow(root_window)->UpdateVisibilityState();
}

gfx::Rect ImmersiveContextAsh::GetDisplayBoundsInScreen(views::Widget* widget) {
  WmWindow* window = WmWindow::Get(widget->GetNativeWindow());
  return window->GetDisplayNearestWindow().bounds();
}

void ImmersiveContextAsh::AddPointerWatcher(
    views::PointerWatcher* watcher,
    views::PointerWatcherEventTypes events) {
  WmShell::Get()->AddPointerWatcher(watcher, events);
}

void ImmersiveContextAsh::RemovePointerWatcher(views::PointerWatcher* watcher) {
  WmShell::Get()->RemovePointerWatcher(watcher);
}

bool ImmersiveContextAsh::DoesAnyWindowHaveCapture() {
  return wm::GetCaptureWindow() != nullptr;
}

bool ImmersiveContextAsh::IsMouseEventsEnabled() {
  return WmShell::Get()->IsMouseEventsEnabled();
}

}  // namespace ash
