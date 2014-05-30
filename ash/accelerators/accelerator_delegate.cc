// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_delegate.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {}  // namespace

AcceleratorDelegate::AcceleratorDelegate() {
}
AcceleratorDelegate::~AcceleratorDelegate() {
}

void AcceleratorDelegate::PreProcessAccelerator(
    const ui::Accelerator& accelerator) {
  // Fill out context object so AcceleratorController will know what
  // was the previous accelerator or if the current accelerator is repeated.
  Shell::GetInstance()->accelerator_controller()->context()->UpdateContext(
      accelerator);
}

// Uses the top level window so if the target is a web contents window the
// containing parent window will be checked for the property.
bool AcceleratorDelegate::CanConsumeSystemKeys(const ui::KeyEvent& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (!target)  // Can be NULL in tests.
    return false;
  aura::Window* top_level = ::wm::GetToplevelWindow(target);
  return top_level && wm::GetWindowState(top_level)->can_consume_system_keys();
}

// Returns true if the |accelerator| should be processed now, inside Ash's env
// event filter.
bool AcceleratorDelegate::ShouldProcessAcceleratorNow(
    const ui::KeyEvent& event,
    const ui::Accelerator& accelerator) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (!target)
    return true;

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  if (std::find(root_windows.begin(), root_windows.end(), target) !=
      root_windows.end())
    return true;

  // A full screen window should be able to handle all key events including the
  // reserved ones.
  if (wm::GetWindowState(target)->IsFullscreen()) {
    // TODO(yusukes): On Chrome OS, only browser and flash windows can be full
    // screen. Launching an app in "open full-screen" mode is not supported yet.
    // That makes the IsWindowFullscreen() check above almost meaningless
    // because a browser and flash window do handle Ash accelerators anyway
    // before they're passed to a page or flash content.
    return false;
  }

  if (Shell::GetInstance()->GetAppListTargetVisibility())
    return true;

  // Unless |target| is in the full screen state, handle reserved accelerators
  // such as Alt+Tab now.
  return Shell::GetInstance()->accelerator_controller()->IsReservedAccelerator(
      accelerator);
}

bool AcceleratorDelegate::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  return Shell::GetInstance()->accelerator_controller()->Process(accelerator);
}

}  // namespace ash
