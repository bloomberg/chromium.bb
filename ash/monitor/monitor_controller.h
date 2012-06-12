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
#include "ui/aura/display_observer.h"
#include "ui/aura/monitor_manager.h"

namespace aura {
class Display;
class RootWindow;
}

namespace ash {
namespace internal {

// MonitorController creates and maintains RootWindows for each
// attached monitor, keeping them in sync with monitor configuration changes.
class MonitorController : public aura::DisplayObserver {
 public:
  MonitorController();
  virtual ~MonitorController();

  // Gets all of the root windows.
  void GetAllRootWindows(std::vector<aura::RootWindow*>* windows);

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

 private:
  void Init();

  std::map<int, aura::RootWindow*> root_windows_;

  DISALLOW_COPY_AND_ASSIGN(MonitorController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MONITOR_MONITOR_CONTROLLER_H_
