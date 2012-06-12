// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/monitor/multi_monitor_manager.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"

namespace ash {
namespace internal {

MonitorController::MonitorController() {
  aura::Env::GetInstance()->monitor_manager()->AddObserver(this);
  Init();
}

MonitorController::~MonitorController() {
  aura::Env::GetInstance()->monitor_manager()->RemoveObserver(this);
  // Remove the root first.
  int monitor_id = Shell::GetPrimaryRootWindow()->GetProperty(kMonitorIdKey);
  DCHECK(monitor_id >= 0);
  root_windows_.erase(monitor_id);
  STLDeleteContainerPairSecondPointers(
      root_windows_.begin(), root_windows_.end());
}

void MonitorController::GetAllRootWindows(
    std::vector<aura::RootWindow*>* windows) {
  DCHECK(windows);
  windows->clear();

  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it)
    windows->push_back(it->second);
}

void MonitorController::OnDisplayBoundsChanged(const gfx::Display& display) {
  root_windows_[display.id()]->SetHostBounds(display.bounds_in_pixel());
}

void MonitorController::OnDisplayAdded(const gfx::Display& display) {
  if (root_windows_.empty()) {
    root_windows_[display.id()] = Shell::GetPrimaryRootWindow();
    Shell::GetPrimaryRootWindow()->SetHostBounds(display.bounds_in_pixel());
    return;
  }
  aura::RootWindow* root = aura::Env::GetInstance()->monitor_manager()->
      CreateRootWindowForMonitor(display);
  root_windows_[display.id()] = root;
  Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
}

void MonitorController::OnDisplayRemoved(const gfx::Display& display) {
  aura::RootWindow* root = root_windows_[display.id()];
  DCHECK(root);
  // Primary monitor should never be removed by MonitorManager.
  DCHECK(root != Shell::GetPrimaryRootWindow());
  // Monitor for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != Shell::GetPrimaryRootWindow()) {
    root_windows_.erase(display.id());
    delete root;
  }
}

void MonitorController::Init() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  for (size_t i = 0; i < monitor_manager->GetNumMonitors(); ++i) {
    const gfx::Display& display = monitor_manager->GetMonitorAt(i);
    if (i == 0) {
      // Primary monitor
      root_windows_[display.id()] = Shell::GetPrimaryRootWindow();
      Shell::GetPrimaryRootWindow()->SetHostBounds(display.bounds_in_pixel());
    } else {
      aura::RootWindow* root =
          monitor_manager->CreateRootWindowForMonitor(display);
      root_windows_[display.id()] = root;
      Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
    }
  }
}

}  // namespace internal
}  // namespace ash
