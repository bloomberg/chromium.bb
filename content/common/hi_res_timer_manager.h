// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_HI_RES_TIMER_MANAGER_H_
#define CONTENT_COMMON_HI_RES_TIMER_MANAGER_H_
#pragma once

#include "ui/base/system_monitor/system_monitor.h"

// Ensures that the Windows high resolution timer is only used
// when not running on battery power.
class HighResolutionTimerManager : public ui::SystemMonitor::PowerObserver {
 public:
  HighResolutionTimerManager();
  virtual ~HighResolutionTimerManager();

  // ui::SystemMonitor::PowerObserver:
  virtual void OnPowerStateChange(bool on_battery_power);

 private:
  // Enable or disable the faster multimedia timer.
  void UseHiResClock(bool use);

  bool hi_res_clock_used_;

  DISALLOW_COPY_AND_ASSIGN(HighResolutionTimerManager);
};

#endif  // CONTENT_COMMON_HI_RES_TIMER_MANAGER_H_
