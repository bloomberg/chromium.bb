// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_DISPLAY_OBSERVER_H_
#define ASH_COMMON_WM_DISPLAY_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

// Used to track changes in display configuration.
class ASH_EXPORT WmDisplayObserver {
 public:
  // Called prior to the display configuration changing.
  virtual void OnDisplayConfigurationChanging() {}

  // Called after the display configure changed.
  virtual void OnDisplayConfigurationChanged() {}

 protected:
  virtual ~WmDisplayObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_DISPLAY_OBSERVER_H_
