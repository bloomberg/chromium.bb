// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOCK_SCREEN_APPS_FOCUS_OBSERVER_H_
#define ASH_LOGIN_LOCK_SCREEN_APPS_FOCUS_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

// An interface used to observe lock screen apps focus related events, as
// reported through lock screen mojo interface.
class ASH_EXPORT LockScreenAppsFocusObserver {
 public:
  virtual ~LockScreenAppsFocusObserver() {}

  // Called when focus is leaving a lock screen app window due to tabbing.
  // |reverse| - whether the tab order is reversed.
  virtual void OnFocusLeavingLockScreenApps(bool reverse) = 0;
};

}  // namespace ash

#endif  // ASH_LOGIN_LOCK_SCREEN_APPS_FOCUS_OBSERVER_H_
