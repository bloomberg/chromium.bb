// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
#define ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

// MultiMonitorManager maintains the current monitor configurations,
// and notifies observers when configuration changes.
// This is exported for unittest.
//
// TODO(oshima): gfx::Screen needs to return translated coordinates
// if the root window is translated. crbug.com/119268.
class ASH_EXPORT MultiMonitorManager : public aura::MonitorManager,
                                       public aura::RootWindowObserver {
 public:
  MultiMonitorManager();
  virtual ~MultiMonitorManager();

  // Used to emulate monitor change when run in a desktop environment instead
  // of on a device.
  static void AddRemoveMonitor();
  static void CycleMonitor();

  // MonitorManager overrides:
  virtual void OnNativeMonitorsChanged(
      const std::vector<const aura::Monitor*>& monitors) OVERRIDE;
  virtual aura::RootWindow* CreateRootWindowForMonitor(
      aura::Monitor* monitor) OVERRIDE;
  virtual const aura::Monitor* GetMonitorNearestWindow(
      const aura::Window* window) const OVERRIDE;
  virtual const aura::Monitor* GetMonitorNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual aura::Monitor* GetMonitorAt(size_t index) OVERRIDE;
  virtual size_t GetNumMonitors() const OVERRIDE;
  virtual aura::Monitor* GetMonitorNearestWindow(
      const aura::Window* window) OVERRIDE;

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& new_size) OVERRIDE;

 private:
  typedef std::vector<aura::Monitor*> Monitors;

  void Init();
  void AddRemoveMonitorImpl();
  void CycleMonitorImpl();

  Monitors monitors_;

  DISALLOW_COPY_AND_ASSIGN(MultiMonitorManager);
};

extern const aura::WindowProperty<aura::Monitor*>* const kMonitorKey;

}  // namespace internal
}  // namespace ash

#endif  //  ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
