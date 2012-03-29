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
namespace {

aura::Monitor* Copy(aura::Monitor* m) {
  aura::Monitor* monitor = new aura::Monitor;
  monitor->set_bounds(m->bounds());
  return monitor;
}

}  // namespace

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

// static
void MultiMonitorManager::AddRemoveMonitor() {
  MultiMonitorManager* manager = static_cast<MultiMonitorManager*>(
      aura::Env::GetInstance()->monitor_manager());
  manager->AddRemoveMonitorImpl();
}

void MultiMonitorManager::CycleMonitor() {
  MultiMonitorManager* manager = static_cast<MultiMonitorManager*>(
      aura::Env::GetInstance()->monitor_manager());
  manager->CycleMonitorImpl();
}

void MultiMonitorManager::OnNativeMonitorsChanged(
    const std::vector<const aura::Monitor*>& new_monitors) {
  size_t min = std::min(monitors_.size(), new_monitors.size());

  // For m19, we only care about 1st monitor as primary, and
  // don't differentiate the rest of monitors as all secondary
  // monitors have the same content.
  // TODO(oshima): Fix this so that we can differentiate outputs
  // and keep a content on one monitor stays on the same monitor
  // when a monitor is added or removed.
  for (size_t i = 0; i < min; ++i) {
    Monitor* current_monitor = monitors_[i];
    const Monitor* new_monitor = new_monitors[i];
    if (current_monitor->bounds() != new_monitor->bounds()) {
      current_monitor->set_bounds(new_monitor->bounds());
      NotifyBoundsChanged(current_monitor);
    }
  }

  if (monitors_.size() < new_monitors.size()) {
    // New monitors added
    for (size_t i = min; i < new_monitors.size(); ++i) {
      Monitor* monitor = new Monitor();
      monitor->set_bounds(new_monitors[i]->bounds());
      monitors_.push_back(monitor);
      NotifyMonitorAdded(monitor);
    }
  } else {
    // Monitors are removed. We keep the monitor for the primary
    // monitor (at index 0) because it needs the monitor information
    // even if it doesn't exit.
    while (monitors_.size() > new_monitors.size() && monitors_.size() > 1) {
      Monitor* monitor = monitors_.back();
      NotifyMonitorRemoved(monitor);
      monitors_.erase(std::find(monitors_.begin(), monitors_.end(), monitor));
      delete monitor;
    }
  }
}

RootWindow* MultiMonitorManager::CreateRootWindowForMonitor(
    Monitor* monitor) {
  RootWindow* root_window = new RootWindow(monitor->bounds());
  // No need to remove RootWindowObserver because
  // the MonitorManager object outlives RootWindow objects.
  root_window->AddRootWindowObserver(this);
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

void MultiMonitorManager::OnRootWindowResized(const aura::RootWindow* root,
                                              const gfx::Size& old_size) {
  if (!use_fullscreen_host_window()) {
    Monitor* monitor = root->GetProperty(kMonitorKey);
    monitor->set_size(root->GetHostSize());
    NotifyBoundsChanged(monitor);
  }
}

void MultiMonitorManager::Init() {
  // TODO(oshima): Move this logic to MonitorChangeObserver.
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

void MultiMonitorManager::AddRemoveMonitorImpl() {
  std::vector<const Monitor*> new_monitors;
  if (monitors_.size() > 1) {
    // Remove if there is more than one monitor.
    int count = monitors_.size() - 1;
    for (Monitors::const_iterator iter = monitors_.begin(); count-- > 0; ++iter)
      new_monitors.push_back(Copy(*iter));
  } else {
    // Add if there is only one monitor.
    new_monitors.push_back(Copy(monitors_[0]));
    aura::Monitor* extra_monitor = new Monitor;
    extra_monitor->set_bounds(gfx::Rect(100, 100, 1440, 800));
    new_monitors.push_back(extra_monitor);
  }
  if (new_monitors.size())
    OnNativeMonitorsChanged(new_monitors);
  STLDeleteContainerPointers(new_monitors.begin(), new_monitors.end());
}

void MultiMonitorManager::CycleMonitorImpl() {
  if (monitors_.size() > 1) {
    std::vector<const Monitor*> new_monitors;
    for (Monitors::const_iterator iter = monitors_.begin() + 1;
         iter != monitors_.end(); ++iter)
      new_monitors.push_back(Copy(*iter));
    new_monitors.push_back(Copy(monitors_.front()));
    OnNativeMonitorsChanged(new_monitors);
    STLDeleteContainerPointers(new_monitors.begin(), new_monitors.end());
  }
}

}  // namespace internal
}  // namespace ash
