// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/hi_res_timer_manager.h"

// On POSIX we don't need to do anything special with the system timer.

HighResolutionTimerManager::HighResolutionTimerManager() {
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
}

void HighResolutionTimerManager::OnPowerStateChange(bool on_battery_power) {
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
}
