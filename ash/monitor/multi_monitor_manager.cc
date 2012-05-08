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
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/rect.h"

DECLARE_WINDOW_PROPERTY_TYPE(int);

namespace ash {
namespace internal {
namespace {
gfx::Monitor& GetInvalidMonitor() {
  static gfx::Monitor* invalid_monitor = new gfx::Monitor();
  return *invalid_monitor;
}
}  // namespace

using aura::RootWindow;
using aura::Window;
using gfx::Monitor;
using std::string;
using std::vector;

DEFINE_WINDOW_PROPERTY_KEY(int, kMonitorIdKey, -1);

MultiMonitorManager::MultiMonitorManager() {
  Init();
}

MultiMonitorManager::~MultiMonitorManager() {
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

  void MultiMonitorManager::ToggleMonitorScale() {
  MultiMonitorManager* manager = static_cast<MultiMonitorManager*>(
      aura::Env::GetInstance()->monitor_manager());
  manager->ScaleMonitorImpl();
}

void MultiMonitorManager::OnNativeMonitorsChanged(
    const std::vector<Monitor>& new_monitors) {
  size_t min = std::min(monitors_.size(), new_monitors.size());

  // For m19, we only care about 1st monitor as primary, and
  // don't differentiate the rest of monitors as all secondary
  // monitors have the same content. ID for primary monitor stays the same
  // because we never remove it, we don't update IDs for other monitors
  // , for now, because they're the same.
  // TODO(oshima): Fix this so that we can differentiate outputs
  // and keep a content on one monitor stays on the same monitor
  // when a monitor is added or removed.
  for (size_t i = 0; i < min; ++i) {
    Monitor& current_monitor = monitors_[i];
    const Monitor& new_monitor = new_monitors[i];
    if (current_monitor.bounds_in_pixel() != new_monitor.bounds_in_pixel()) {
      current_monitor.SetScaleAndBounds(new_monitor.device_scale_factor(),
                                        new_monitor.bounds_in_pixel());
      NotifyBoundsChanged(current_monitor);
    }
  }

  if (monitors_.size() < new_monitors.size()) {
    // New monitors added
    for (size_t i = min; i < new_monitors.size(); ++i) {
      const gfx::Monitor& new_monitor = new_monitors[i];
      monitors_.push_back(Monitor(new_monitor.id()));
      gfx::Monitor& monitor = monitors_.back();
      monitor.SetScaleAndBounds(new_monitor.device_scale_factor(),
                                new_monitor.bounds_in_pixel());
      NotifyMonitorAdded(monitor);
    }
  } else {
    // Monitors are removed. We keep the monitor for the primary
    // monitor (at index 0) because it needs the monitor information
    // even if it doesn't exit.
    while (monitors_.size() > new_monitors.size() && monitors_.size() > 1) {
      Monitors::reverse_iterator iter = monitors_.rbegin();
      NotifyMonitorRemoved(*iter);
      monitors_.erase(iter.base() - 1);
    }
  }
}

RootWindow* MultiMonitorManager::CreateRootWindowForMonitor(
    const Monitor& monitor) {
  RootWindow* root_window = new RootWindow(monitor.bounds_in_pixel());
  // No need to remove RootWindowObserver because
  // the MonitorManager object outlives RootWindow objects.
  root_window->AddRootWindowObserver(this);
  root_window->SetProperty(kMonitorIdKey, monitor.id());
  root_window->Init();
  return root_window;
}

const Monitor& MultiMonitorManager::GetMonitorAt(size_t index) {
  return index < monitors_.size() ? monitors_[index] : GetInvalidMonitor();
}

size_t MultiMonitorManager::GetNumMonitors() const {
  return monitors_.size();
}

const Monitor& MultiMonitorManager::GetMonitorNearestWindow(
    const Window* window) const {
  if (!window) {
    MultiMonitorManager* manager = const_cast<MultiMonitorManager*>(this);
    return manager->GetMonitorAt(0);
  }
  const RootWindow* root = window->GetRootWindow();
  MultiMonitorManager* that = const_cast<MultiMonitorManager*>(this);
  return root ?
      that->FindMonitorById(root->GetProperty(kMonitorIdKey)) :
      GetInvalidMonitor();
}

const Monitor& MultiMonitorManager::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  // TODO(oshima): For m19, mouse is constrained within
  // the primary window.
  MultiMonitorManager* manager = const_cast<MultiMonitorManager*>(this);
  return manager->GetMonitorAt(0);
}

void MultiMonitorManager::OnRootWindowResized(const aura::RootWindow* root,
                                              const gfx::Size& old_size) {
  if (!use_fullscreen_host_window()) {
    int monitor_id = root->GetProperty(kMonitorIdKey);
    Monitor& monitor = FindMonitorById(monitor_id);
    monitor.SetSize(root->GetHostSize());
    NotifyBoundsChanged(monitor);
  }
}

bool MultiMonitorManager::UpdateWorkAreaOfMonitorNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const RootWindow* root = window->GetRootWindow();
  Monitor& monitor = FindMonitorById(root->GetProperty(kMonitorIdKey));
  gfx::Rect old_work_area = monitor.work_area();
  monitor.UpdateWorkAreaFromInsets(insets);
  return old_work_area != monitor.work_area();
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
  std::vector<Monitor> new_monitors;
  if (monitors_.size() > 1) {
    // Remove if there is more than one monitor.
    int count = monitors_.size() - 1;
    for (Monitors::const_iterator iter = monitors_.begin(); count-- > 0; ++iter)
      new_monitors.push_back(*iter);
  } else {
    // Add if there is only one monitor.
    new_monitors.push_back(monitors_[0]);
    new_monitors.push_back(CreateMonitorFromSpec("50+50-1280x768"));
  }
  if (new_monitors.size())
    OnNativeMonitorsChanged(new_monitors);
}

void MultiMonitorManager::CycleMonitorImpl() {
  if (monitors_.size() > 1) {
    std::vector<Monitor> new_monitors;
    for (Monitors::const_iterator iter = monitors_.begin() + 1;
         iter != monitors_.end(); ++iter) {
      gfx::Monitor monitor = *iter;
      new_monitors.push_back(monitor);
    }
    new_monitors.push_back(monitors_.front());
    OnNativeMonitorsChanged(new_monitors);
  }
}

void MultiMonitorManager::ScaleMonitorImpl() {
  if (monitors_.size() > 0) {
    std::vector<Monitor> new_monitors;
    for (Monitors::const_iterator iter = monitors_.begin();
         iter != monitors_.end(); ++iter) {
      gfx::Monitor monitor = *iter;
      float factor = monitor.device_scale_factor() == 1.0f ? 2.0f : 1.0f;
      monitor.SetScaleAndBounds(
          factor, gfx::Rect(monitor.bounds_in_pixel().origin(),
                            monitor.size().Scale(factor)));
      new_monitors.push_back(monitor);
    }
    OnNativeMonitorsChanged(new_monitors);
  }
}

gfx::Monitor& MultiMonitorManager::FindMonitorById(int id) {
  for (Monitors::iterator iter = monitors_.begin();
       iter != monitors_.end(); ++iter) {
    if ((*iter).id() == id)
      return *iter;
  }
  DLOG(FATAL) << "Could not find monitor by id:" << id;
  return GetInvalidMonitor();
}

}  // namespace internal
}  // namespace ash
