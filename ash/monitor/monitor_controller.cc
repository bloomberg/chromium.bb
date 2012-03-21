// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/monitor/multi_monitor_manager.h"
#include "ash/monitor/secondary_monitor_view.h"
#include "ash/shell.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/stl_util.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

namespace {

void SetupAsSecondaryMonitor(aura::RootWindow* root) {
  root->SetLayoutManager(new internal::RootWindowLayoutManager(root));
  aura::Window* container = new aura::Window(NULL);
  container->SetName("SecondaryMonitorContainer");
  container->Init(ui::Layer::LAYER_NOT_DRAWN);
  root->AddChild(container);
  container->Show();
  container->SetLayoutManager(new internal::BaseLayoutManager(root));
  CreateSecondaryMonitorWidget(container);
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
  aura::Monitor* monitor = Shell::GetRootWindow()->GetProperty(kMonitorKey);
  root_windows_.erase(monitor);
  STLDeleteContainerPairSecondPointers(
      root_windows_.begin(), root_windows_.end());
}

void MonitorController::OnMonitorBoundsChanged(const aura::Monitor* monitor) {
  if (aura::MonitorManager::use_fullscreen_host_window())
    root_windows_[monitor]->SetHostBounds(monitor->bounds());
}

void MonitorController::Init() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  for (size_t i = 0; i < monitor_manager->GetNumMonitors(); ++i) {
    aura::Monitor* monitor = monitor_manager->GetMonitorAt(i);
    const aura::Monitor* key = monitor;
    if (i == 0) {
      // Primary monitor
      root_windows_[key] = Shell::GetRootWindow();
    } else {
      aura::RootWindow* root =
          monitor_manager->CreateRootWindowForMonitor(monitor);
      root_windows_[key] = root;
      SetupAsSecondaryMonitor(root);
    }
  }
}

}  // namespace internal
}  // namespace ash
