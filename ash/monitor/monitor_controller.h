// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MONITOR_CONTROLLER_H_
#define ASH_MONITOR_MONITOR_CONTROLLER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/monitor_manager.h"

namespace aura {
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

  // aura::MonitorObserver overrides:
  virtual void OnMonitorBoundsChanged(const aura::Monitor* monitor) OVERRIDE;
  virtual void OnMonitorAdded(aura::Monitor* monitor) OVERRIDE;
  virtual void OnMonitorRemoved(const aura::Monitor* monitor) OVERRIDE;

 private:
  void Init();

  std::map<const aura::Monitor*, aura::RootWindow*> root_windows_;

  DISALLOW_COPY_AND_ASSIGN(MonitorController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MONITOR_MONITOR_CONTROLLER_H_
