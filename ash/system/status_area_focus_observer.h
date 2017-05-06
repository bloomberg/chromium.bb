// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_FOCUS_OBSERVER_H_
#define ASH_SYSTEM_STATUS_AREA_FOCUS_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// A class that observes status-area-related events.
class ASH_EXPORT StatusAreaFocusObserver {
 public:
  // Called if the status area is going to focus out.
  virtual void OnFocusOut(bool reverse) = 0;

 protected:
  virtual ~StatusAreaFocusObserver() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_FOCUS_OBSERVER_H_
