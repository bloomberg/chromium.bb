// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/monitor/multi_monitor_manager.h"
#include "ash/monitor/secondary_monitor_view.h"
#include "ash/shell.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/monitor.h"

namespace ash {
namespace internal {

namespace {

void SetupAsSecondaryMonitor(aura::RootWindow* root) {
  root->SetFocusWhenShown(false);
  root->SetLayoutManager(new internal::RootWindowLayoutManager(root));
  aura::Window* container = new aura::Window(NULL);
  container->SetName("SecondaryMonitorContainer");
  container->Init(ui::LAYER_NOT_DRAWN);
  root->AddChild(container);
  container->SetLayoutManager(new internal::BaseLayoutManager(root));
  CreateSecondaryMonitorWidget(container);
  container->Show();
  root->layout_manager()->OnWindowResized();
  root->ShowRootWindow();
}

}  // namespace

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
  SetupAsSecondaryMonitor(root);
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
      SetupAsSecondaryMonitor(root);
    }
  }
}

}  // namespace internal
}  // namespace ash
