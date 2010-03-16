// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include <CoreGraphics/CGEventSource.h>

IdleState CalculateIdleState(unsigned int idle_threshold) {
  unsigned int idle_time = CGEventSourceSecondsSinceLastEventType(
     kCGEventSourceStateCombinedSessionState,
     kCGAnyInputEventType);
  if (idle_time >= idle_threshold)
    return IDLE_STATE_IDLE;
  return IDLE_STATE_ACTIVE;
}
