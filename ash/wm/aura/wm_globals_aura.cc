// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_globals_aura.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace wm {
namespace {

WmGlobalsAura* instance_ = nullptr;

}  // namespace

// static
WmGlobals* WmGlobals::Get() {
  return instance_;
}

WmGlobalsAura::WmGlobalsAura() {
  DCHECK(!instance_);
  instance_ = this;
}

WmGlobalsAura::~WmGlobalsAura() {
  instance_ = nullptr;
}

// static
WmGlobalsAura* WmGlobalsAura::Get() {
  if (!instance_)
    new WmGlobalsAura;
  return instance_;
}

WmWindow* WmGlobalsAura::GetActiveWindow() {
  return WmWindowAura::Get(wm::GetActiveWindow());
}

WmWindow* WmGlobalsAura::GetRootWindowForDisplayId(int64_t display_id) {
  return WmWindowAura::Get(Shell::GetInstance()
                               ->window_tree_host_manager()
                               ->GetRootWindowForDisplayId(display_id));
}

WmWindow* WmGlobalsAura::GetRootWindowForNewWindows() {
  return WmWindowAura::Get(Shell::GetTargetRootWindow());
}

std::vector<WmWindow*> WmGlobalsAura::GetMruWindowListIgnoreModals() {
  const std::vector<aura::Window*> windows = ash::Shell::GetInstance()
                                                 ->mru_window_tracker()
                                                 ->BuildWindowListIgnoreModal();
  std::vector<WmWindow*> wm_windows(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    wm_windows[i] = WmWindowAura::Get(windows[i]);
  return wm_windows;
}

bool WmGlobalsAura::IsForceMaximizeOnFirstRun() {
  return Shell::GetInstance()->delegate()->IsForceMaximizeOnFirstRun();
}

std::vector<WmWindow*> WmGlobalsAura::GetAllRootWindows() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::vector<WmWindow*> wm_windows(root_windows.size());
  for (size_t i = 0; i < root_windows.size(); ++i)
    wm_windows[i] = WmWindowAura::Get(root_windows[i]);
  return wm_windows;
}

}  // namespace wm
}  // namespace ash
