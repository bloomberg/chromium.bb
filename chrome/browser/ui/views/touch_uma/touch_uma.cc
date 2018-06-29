// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/touch_uma/touch_uma.h"

#include "base/metrics/histogram_macros.h"

// static
void TouchUMA::RecordGestureAction(GestureActionType action) {
  UMA_HISTOGRAM_ENUMERATION("Event.Touch.GestureTarget", action);
}
