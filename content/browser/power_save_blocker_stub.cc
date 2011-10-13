// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker.h"

// Default, stub implementation, for platforms that don't have their own yet.

// Called only from UI thread.
// static
void PowerSaveBlocker::ApplyBlock(PowerSaveBlockerType type) {
  // http://code.google.com/p/chromium/issues/detail?id=33605
}
