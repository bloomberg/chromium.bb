// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MONITOR_CONTROLLER_H_
#define ASH_MONITOR_MONITOR_CONTROLLER_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/monitor_observer.h"

namespace aura {
class Monitor;
class RootWindow;
}

namespace ash {
namespace internal {

// MonitorController creates and maintains RootWindows for each
// attached monitor, keeping them in sync with monitor configuration changes.
class MonitorController : public aura::MonitorObserver {
 public:
  MonitorController();
  virtual ~MonitorController();

  // Gets all of the root windows.
  void GetAllRootWindows(std::vector<aura::RootWindow*>* windows);

  // aura::MonitorObserver overrides:
  virtual void OnMonitorBoundsChanged(
      const gfx::Monitor& monitor) OVERRIDE;
  virtual void OnMonitorAdded(const gfx::Monitor& monitor) OVERRIDE;
  virtual void OnMonitorRemoved(const gfx::Monitor& monitor) OVERRIDE;

 private:
  void Init();

  std::map<int, aura::RootWindow*> root_windows_;

  DISALLOW_COPY_AND_ASSIGN(MonitorController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MONITOR_MONITOR_CONTROLLER_H_
