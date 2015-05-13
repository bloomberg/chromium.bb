// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_OBSERVER_H_

#include "components/proximity_auth/screenlock_state.h"

class EasyUnlockServiceObserver {
 public:
  // Invoked when turn-off operation status changes.
  virtual void OnTurnOffOperationStatusChanged() {}

  // Invoked when screenlock state changes.
  virtual void OnScreenlockStateChanged(
      proximity_auth::ScreenlockState state) {}

 protected:
  virtual ~EasyUnlockServiceObserver() {}
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_OBSERVER_H_
