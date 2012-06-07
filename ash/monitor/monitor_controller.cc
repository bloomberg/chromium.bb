// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/monitor/multi_monitor_manager.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/monitor.h"

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

void MonitorController::OnMonitorBoundsChanged(const gfx::Monitor& monitor) {
  root_windows_[monitor.id()]->SetHostBounds(monitor.bounds_in_pixel());
}

void MonitorController::OnMonitorAdded(const gfx::Monitor& monitor) {
  if (root_windows_.empty()) {
    root_windows_[monitor.id()] = Shell::GetPrimaryRootWindow();
    Shell::GetPrimaryRootWindow()->SetHostBounds(monitor.bounds_in_pixel());
    return;
  }
  aura::RootWindow* root = aura::Env::GetInstance()->monitor_manager()->
      CreateRootWindowForMonitor(monitor);
  root_windows_[monitor.id()] = root;
  Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
}

void MonitorController::OnMonitorRemoved(const gfx::Monitor& monitor) {
  aura::RootWindow* root = root_windows_[monitor.id()];
  DCHECK(root);
  // Primary monitor should never be removed by MonitorManager.
  DCHECK(root != Shell::GetPrimaryRootWindow());
  // Monitor for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != Shell::GetPrimaryRootWindow()) {
    root_windows_.erase(monitor.id());
    delete root;
  }
}

void MonitorController::Init() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  for (size_t i = 0; i < monitor_manager->GetNumMonitors(); ++i) {
    const gfx::Monitor& monitor = monitor_manager->GetMonitorAt(i);
    if (i == 0) {
      // Primary monitor
      root_windows_[monitor.id()] = Shell::GetPrimaryRootWindow();
      Shell::GetPrimaryRootWindow()->SetHostBounds(monitor.bounds_in_pixel());
    } else {
      aura::RootWindow* root =
          monitor_manager->CreateRootWindowForMonitor(monitor);
      root_windows_[monitor.id()] = root;
      Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
    }
  }
}

}  // namespace internal
}  // namespace ash
