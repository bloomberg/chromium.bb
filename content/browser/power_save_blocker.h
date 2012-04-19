// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
#define CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"

// A RAII-style class to block the system from entering low-power (sleep) mode.
class CONTENT_EXPORT PowerSaveBlocker {
 public:
  enum PowerSaveBlockerType {
    kPowerSaveBlockPreventNone = -1,

    // Prevent the system from going to sleep; allow display sleep.
    kPowerSaveBlockPreventSystemSleep,

    // Prevent the system or display from going to sleep.
    kPowerSaveBlockPreventDisplaySleep,

    // Count of the values; not valid as a parameter.
    kPowerSaveBlockPreventStateCount
  };

  // Pass in the level of sleep prevention desired. kPowerSaveBlockPreventNone
  // is not a valid option.
  explicit PowerSaveBlocker(PowerSaveBlockerType type);
  ~PowerSaveBlocker();

 private:
  // Platform-specific function called when enable state is changed.
  // Guaranteed to be called only from the UI thread.
  static void ApplyBlock(PowerSaveBlockerType type);

  // Called only from UI thread.
  static void AdjustBlockCount(const std::vector<int>& deltas);

  // Invokes AdjustBlockCount on the UI thread.
  static void PostAdjustBlockCount(const std::vector<int>& deltas);

  // Returns the highest-severity block type in use.
  static PowerSaveBlockerType HighestBlockType();

  PowerSaveBlockerType type_;

  static int blocker_count_[];

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlocker);
};

#endif  // CONTENT_BROWSER_POWER_SAVE_BLOCKER_H_
