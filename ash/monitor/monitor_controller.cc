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
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

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
  aura::Monitor* monitor = Shell::GetRootWindow()->GetProperty(kMonitorKey);
  DCHECK(monitor);
  root_windows_.erase(monitor);
  STLDeleteContainerPairSecondPointers(
      root_windows_.begin(), root_windows_.end());
}

void MonitorController::OnMonitorBoundsChanged(const aura::Monitor* monitor) {
  root_windows_[monitor]->SetHostBounds(monitor->bounds());
}

void MonitorController::OnMonitorAdded(aura::Monitor* monitor) {
  if (root_windows_.empty()) {
    root_windows_[monitor] = Shell::GetRootWindow();
    Shell::GetRootWindow()->SetHostBounds(monitor->bounds());
    return;
  }
  aura::RootWindow* root = aura::Env::GetInstance()->monitor_manager()->
      CreateRootWindowForMonitor(monitor);
  root_windows_[monitor] = root;
  SetupAsSecondaryMonitor(root);
}

void MonitorController::OnMonitorRemoved(const aura::Monitor* monitor) {
  aura::RootWindow* root = root_windows_[monitor];
  DCHECK(root);
  // Monitor for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != Shell::GetRootWindow()) {
    root_windows_.erase(monitor);
    delete root;
  }
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
      Shell::GetRootWindow()->SetHostBounds(monitor->bounds());
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
