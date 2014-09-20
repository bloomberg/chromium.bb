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

bool AcceleratorDelegate::ProcessAccelerator(const ui::KeyEvent& key_event,
                                             const ui::Accelerator& accelerator,
                                             KeyType key_type) {
  // Special hardware keys like brightness and volume are handled in
  // special way. However, some windows can override this behavior
  // (e.g. Chrome v1 apps by default and Chrome v2 apps with
  // permission) by setting a window property.
  if (key_type == KEY_TYPE_SYSTEM && !CanConsumeSystemKeys(key_event)) {
    // System keys are always consumed regardless of whether they trigger an
    // accelerator to prevent windows from seeing unexpected key up events.
    Shell::GetInstance()->accelerator_controller()->Process(accelerator);
    return true;
  }
  if (!ShouldProcessAcceleratorNow(key_event, accelerator))
    return false;
  return Shell::GetInstance()->accelerator_controller()->Process(accelerator);
}

// Uses the top level window so if the target is a web contents window the
// containing parent window will be checked for the property.
bool AcceleratorDelegate::CanConsumeSystemKeys(const ui::KeyEvent& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  DCHECK(target);
  aura::Window* top_level = ::wm::GetToplevelWindow(target);
  return top_level && wm::GetWindowState(top_level)->can_consume_system_keys();
}

// Returns true if the |accelerator| should be processed now, inside Ash's env
// event filter.
bool AcceleratorDelegate::ShouldProcessAcceleratorNow(
    const ui::KeyEvent& event,
    const ui::Accelerator& accelerator) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  DCHECK(target);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  if (std::find(root_windows.begin(), root_windows.end(), target) !=
      root_windows.end())
    return true;

  aura::Window* top_level = ::wm::GetToplevelWindow(target);
  Shell* shell = Shell::GetInstance();

  // Reserved accelerators (such as Power button) always have a prority.
  if (shell->accelerator_controller()->IsReserved(accelerator))
    return true;

  // A full screen window has a right to handle all key events including the
  // reserved ones.
  if (top_level && wm::GetWindowState(top_level)->IsFullscreen()) {
    // On ChromeOS, fullscreen windows are either browser or apps, which
    // send key events to a web content first, then will process keys
    // if the web content didn't consume them.
    return false;
  }

  // Handle preferred accelerators (such as ALT-TAB) before sending
  // to the target.
  if (shell->accelerator_controller()->IsPreferred(accelerator))
    return true;

  return shell->GetAppListTargetVisibility();
}

}  // namespace ash
