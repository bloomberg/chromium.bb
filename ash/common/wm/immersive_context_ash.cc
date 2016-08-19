// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/immersive_context_ash.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "base/logging.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

ImmersiveContextAsh::ImmersiveContextAsh() {}

ImmersiveContextAsh::~ImmersiveContextAsh() {}

void ImmersiveContextAsh::InstallResizeHandleWindowTargeter(
    ImmersiveFullscreenController* controller) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(controller->widget());
  window->InstallResizeHandleWindowTargeter(controller);
}

void ImmersiveContextAsh::OnEnteringOrExitingImmersive(
    ImmersiveFullscreenController* controller,
    bool entering) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(controller->widget());
  wm::WindowState* window_state = window->GetWindowState();
  // Auto hide the shelf in immersive fullscreen instead of hiding it.
  window_state->set_shelf_mode_in_fullscreen(
      entering ? ash::wm::WindowState::SHELF_AUTO_HIDE_VISIBLE
               : ash::wm::WindowState::SHELF_HIDDEN);

  // Update the window's immersive mode state for the window manager.
  window_state->set_in_immersive_fullscreen(entering);

  for (WmWindow* root_window : WmShell::Get()->GetAllRootWindows())
    root_window->GetRootWindowController()->GetShelf()->UpdateVisibilityState();
}

gfx::Rect ImmersiveContextAsh::GetDisplayBoundsInScreen(views::Widget* widget) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget);
  return window->GetDisplayNearestWindow().bounds();
}

void ImmersiveContextAsh::AddPointerWatcher(views::PointerWatcher* watcher,
                                            bool wants_moves) {
  WmShell::Get()->AddPointerWatcher(watcher, wants_moves);
}

void ImmersiveContextAsh::RemovePointerWatcher(views::PointerWatcher* watcher) {
  WmShell::Get()->RemovePointerWatcher(watcher);
}

bool ImmersiveContextAsh::DoesAnyWindowHaveCapture() {
  return WmShell::Get()->GetCaptureWindow() != nullptr;
}

bool ImmersiveContextAsh::IsMouseEventsEnabled() {
  return WmShell::Get()->IsMouseEventsEnabled();
}

}  // namespace ash
