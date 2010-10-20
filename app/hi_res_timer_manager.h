// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_HI_RES_TIMER_MANAGER_H_
#define APP_HI_RES_TIMER_MANAGER_H_
#pragma once

#include "app/system_monitor.h"

// Ensures that the Windows high resolution timer is only used
// when not running on battery power.
class HighResolutionTimerManager : public SystemMonitor::PowerObserver {
 public:
  HighResolutionTimerManager();
  virtual ~HighResolutionTimerManager();

  // SystemMonitor::PowerObserver:
  void OnPowerStateChange(bool on_battery_power);

  // Returns true if the hi resolution clock could be used right now.
  bool hi_res_clock_available() const { return hi_res_clock_available_; }

 private:
  // Enable or disable the faster multimedia timer.
  void UseHiResClock(bool use);

  bool hi_res_clock_available_;

  DISALLOW_COPY_AND_ASSIGN(HighResolutionTimerManager);
};

#endif  // APP_HI_RES_TIMER_MANAGER_H_
