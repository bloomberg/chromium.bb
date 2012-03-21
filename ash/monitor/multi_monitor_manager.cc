// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/multi_monitor_manager.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/string_split.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/rect.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::Monitor*);

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(aura::Monitor*, kMonitorKey, NULL);

using std::string;
using std::vector;
using aura::Monitor;
using aura::RootWindow;
using aura::Window;

MultiMonitorManager::MultiMonitorManager() {
  Init();
}

MultiMonitorManager::~MultiMonitorManager() {
  STLDeleteContainerPointers(monitors_.begin(), monitors_.end());
}

void MultiMonitorManager::OnNativeMonitorResized(const gfx::Size& size) {
  // TODO(oshima): Update monitors using xrandr and notify observers
  // Just update the primary for now.
  if (use_fullscreen_host_window()) {
    Monitor* monitor =
        aura::Env::GetInstance()->monitor_manager()->GetMonitorAt(0);
    monitor->set_size(size);
    NotifyBoundsChanged(monitor);
  }
}

RootWindow* MultiMonitorManager::CreateRootWindowForMonitor(
    Monitor* monitor) {
  RootWindow* root_window = new RootWindow(monitor->bounds());
  root_window->AddObserver(this);
  root_window->SetProperty(kMonitorKey, monitor);
  return root_window;
}

const Monitor* MultiMonitorManager::GetMonitorNearestWindow(
    const Window* window) const {
  if (!window) {
    MultiMonitorManager* manager = const_cast<MultiMonitorManager*>(this);
    return manager->GetMonitorAt(0);
  }
  const RootWindow* root = window->GetRootWindow();
  return root ? root->GetProperty(kMonitorKey) : NULL;
}

const Monitor* MultiMonitorManager::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  // TODO(oshima): For m19, mouse is constrained within
  // the primary window.
  MultiMonitorManager* manager = const_cast<MultiMonitorManager*>(this);
  return manager->GetMonitorAt(0);
}

Monitor* MultiMonitorManager::GetMonitorAt(size_t index) {
  return index < monitors_.size() ? monitors_[index] : NULL;
}

size_t MultiMonitorManager::GetNumMonitors() const {
  return monitors_.size();
}

Monitor* MultiMonitorManager::GetMonitorNearestWindow(const Window* window) {
  const MonitorManager* manager = this;
  return const_cast<Monitor*>(manager->GetMonitorNearestWindow(window));
}

void MultiMonitorManager::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& bounds) {
  if (!use_fullscreen_host_window()) {
    Monitor* monitor = window->GetProperty(kMonitorKey);
    monitor->set_size(bounds.size());
    NotifyBoundsChanged(monitor);
  }
}

void MultiMonitorManager::OnWindowDestroying(Window* window) {
  Monitor* monitor = window->GetProperty(kMonitorKey);
  monitors_.erase(std::find(monitors_.begin(), monitors_.end(), monitor));
  delete monitor;
}

void MultiMonitorManager::Init() {
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    monitors_.push_back(CreateMonitorFromSpec(*iter));
  }
  if (monitors_.empty())
    monitors_.push_back(CreateMonitorFromSpec("" /* default */));
}

}  // namespace internal
}  // namespace ash
