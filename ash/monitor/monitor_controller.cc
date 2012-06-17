// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/ash_switches.h"
#include "ash/monitor/multi_monitor_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"

namespace ash {
namespace internal {
namespace {
bool extended_desktop_enabled = false;
}

MonitorController::MonitorController()
    : secondary_display_layout_(RIGHT) {
  aura::Env::GetInstance()->monitor_manager()->AddObserver(this);
}

MonitorController::~MonitorController() {
  aura::Env::GetInstance()->monitor_manager()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (std::map<int, aura::RootWindow*>::const_reverse_iterator it =
           root_windows_.rbegin(); it != root_windows_.rend(); ++it) {
    internal::RootWindowController* controller =
        wm::GetRootWindowController(it->second);
    // RootWindow may not have RootWindowController in non
    // extended desktop mode.
    if (controller)
      delete controller;
    else
      delete it->second;
  }
}

void MonitorController::InitPrimaryDisplay() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  const gfx::Display& display = monitor_manager->GetDisplayAt(0);
  DCHECK_EQ(0, display.id());
  aura::RootWindow* root =
      monitor_manager->CreateRootWindowForMonitor(display);
  root_windows_[display.id()] = root;
  if (aura::MonitorManager::use_fullscreen_host_window() &&
      !IsExtendedDesktopEnabled()) {
    root->ConfineCursorToWindow();
  }
  root->SetHostBounds(display.bounds_in_pixel());
}

void MonitorController::InitSecondaryDisplays() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  for (size_t i = 1; i < monitor_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = monitor_manager->GetDisplayAt(i);
    aura::RootWindow* root =
        monitor_manager->CreateRootWindowForMonitor(display);
    root_windows_[display.id()] = root;
    Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
  }
}

aura::RootWindow* MonitorController::GetPrimaryRootWindow() {
  DCHECK(!root_windows_.empty());
  return root_windows_[0];
}

void MonitorController::CloseChildWindows() {
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    aura::RootWindow* root_window = it->second;
    internal::RootWindowController* controller =
        wm::GetRootWindowController(root_window);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root_window->children().empty()) {
        aura::Window* child = root_window->children()[0];
        delete child;
      }
    }
  }
}

std::vector<aura::RootWindow*> MonitorController::GetAllRootWindows() {
  std::vector<aura::RootWindow*> windows;
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    if (wm::GetRootWindowController(it->second))
      windows.push_back(it->second);
  }
  return windows;
}

std::vector<internal::RootWindowController*>
MonitorController::GetAllRootWindowControllers() {
  std::vector<internal::RootWindowController*> controllers;
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    internal::RootWindowController* controller =
        wm::GetRootWindowController(it->second);
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void MonitorController::SetSecondaryDisplayLayout(
    SecondaryDisplayLayout layout) {
  secondary_display_layout_ = layout;
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
    internal::RootWindowController* controller =
        wm::GetRootWindowController(root);
    if (controller)
      delete controller;
    else
      delete root;
  }
}

// static
bool MonitorController::IsExtendedDesktopEnabled(){
  return extended_desktop_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshExtendedDesktop);
}

// static
void MonitorController::SetExtendedDesktopEnabled(bool enabled) {
  extended_desktop_enabled = enabled;
}

}  // namespace internal
}  // namespace ash
