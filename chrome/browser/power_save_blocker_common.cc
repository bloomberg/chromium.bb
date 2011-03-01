// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/power_save_blocker.h"
#include "content/browser/browser_thread.h"

// Accessed only from the UI thread.
int PowerSaveBlocker::blocker_count_ = 0;

PowerSaveBlocker::PowerSaveBlocker(bool enable) : enabled_(false) {
  if (enable)
    Enable();
}

PowerSaveBlocker::~PowerSaveBlocker(void) {
  Disable();
}

void PowerSaveBlocker::Enable() {
  if (enabled_)
    return;

  enabled_ = true;
  PostAdjustBlockCount(1);
}

void PowerSaveBlocker::Disable() {
  if (!enabled_)
    return;

  enabled_ = false;
  PostAdjustBlockCount(-1);
}


void PowerSaveBlocker::PostAdjustBlockCount(int delta) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&PowerSaveBlocker::AdjustBlockCount, delta));
}

// Called only from UI thread.
void PowerSaveBlocker::AdjustBlockCount(int delta) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool was_blocking = (blocker_count_ != 0);

  blocker_count_ += delta;

  bool is_blocking = (blocker_count_ != 0);

  DCHECK_GE(blocker_count_, 0);

  if (is_blocking != was_blocking)
    ApplyBlock(is_blocking);
}
