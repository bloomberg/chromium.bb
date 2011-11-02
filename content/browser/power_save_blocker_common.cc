// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Accessed only from the UI thread.
int PowerSaveBlocker::blocker_count_[kPowerSaveBlockPreventStateCount];

PowerSaveBlocker::PowerSaveBlocker(PowerSaveBlockerType type) : type_(type) {
  DCHECK_LT(kPowerSaveBlockPreventNone, type);
  DCHECK_GT(kPowerSaveBlockPreventStateCount, type);
  std::vector<int> change(kPowerSaveBlockPreventStateCount);
  ++change[type_];
  PostAdjustBlockCount(change);
}

PowerSaveBlocker::~PowerSaveBlocker(void) {
  std::vector<int> change(kPowerSaveBlockPreventStateCount);
  --change[type_];
  PostAdjustBlockCount(change);
}

// static
void PowerSaveBlocker::PostAdjustBlockCount(const std::vector<int>& deltas) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PowerSaveBlocker::AdjustBlockCount, deltas));
}

// Called only from UI thread.
// static
void PowerSaveBlocker::AdjustBlockCount(const std::vector<int>& deltas) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PowerSaveBlockerType old_block = HighestBlockType();

  for (size_t i = 0; i < deltas.size(); ++i)
    blocker_count_[i] += deltas[i];

  PowerSaveBlockerType new_block = HighestBlockType();

  if (new_block != old_block)
    ApplyBlock(new_block);
}

// Called only from UI thread.
PowerSaveBlocker::PowerSaveBlockerType PowerSaveBlocker::HighestBlockType() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (PowerSaveBlockerType t = kPowerSaveBlockPreventDisplaySleep;
       t >= kPowerSaveBlockPreventSystemSleep;
       t = static_cast<PowerSaveBlockerType>(t - 1)) {
    if (blocker_count_[t])
      return t;
  }

  return kPowerSaveBlockPreventNone;
}
