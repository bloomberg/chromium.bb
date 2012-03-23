// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
#define ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace internal {

class MultiMonitorManager : public aura::MonitorManager,
                            public aura::WindowObserver {
 public:
  MultiMonitorManager();
  virtual ~MultiMonitorManager();

  // MonitorManager overrides:
  virtual void OnNativeMonitorResized(const gfx::Size& size) OVERRIDE;
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

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& bounds) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  typedef std::vector<aura::Monitor*> Monitors;

  void Init();

  Monitors monitors_;

  DISALLOW_COPY_AND_ASSIGN(MultiMonitorManager);
};

extern const aura::WindowProperty<aura::Monitor*>* const kMonitorKey;

}  // namespace internal
}  // namespace ash

#endif  //  ASH_MONITOR_MULTI_MONITOR_MANAGER_H_
