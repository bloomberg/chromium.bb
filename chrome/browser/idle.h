// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IDLE_H_
#define CHROME_BROWSER_IDLE_H_
#pragma once

#include "base/callback.h"

enum IdleState {
  IDLE_STATE_ACTIVE = 0,
  IDLE_STATE_IDLE = 1,   // No activity within threshold.
  IDLE_STATE_LOCKED = 2,  // Only available on supported systems.
  IDLE_STATE_UNKNOWN = 3  // Used when waiting for the Idle state or in error
                          // conditions
};

// For MacOSX, InitIdleMonitor needs to be called first to setup the monitor.
// StopIdleMonitor should be called if it is not needed any more.
#if defined(OS_MACOSX)
void InitIdleMonitor();
void StopIdleMonitor();
#endif

typedef base::Callback<void(IdleState)> IdleCallback;

// Calculate the Idle state and notify the callback.
void CalculateIdleState(unsigned int idle_threshold, IdleCallback notify);

// Checks synchronously if Idle state is IDLE_STATE_LOCKED.
bool CheckIdleStateIsLocked();
#endif  // CHROME_BROWSER_IDLE_H_
