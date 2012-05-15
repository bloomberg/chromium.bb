// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
#pragma once

#include "base/time.h"

namespace chromeos {

class UpdateScreenActor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Force cancel update.
    virtual void CancelUpdate() = 0;
    virtual void OnActorDestroyed(UpdateScreenActor* actor) = 0;
  };

  virtual ~UpdateScreenActor() {}

  // Sets screen this actor belongs to.
  virtual void SetDelegate(Delegate* screen) = 0;

  // Shows the screen.
  virtual void Show() = 0;

  // Hides the screen.
  virtual void Hide() = 0;

  virtual void PrepareToShow() = 0;

  // Shows manual reboot info message.
  virtual void ShowManualRebootInfo() = 0;

  // Sets current progress in percents.
  virtual void SetProgress(int progress) = 0;

  // Shows estimated time left message.
  virtual void ShowEstimatedTimeLeft(bool enable) = 0;

  // Sets current estimation for time left in the downloading stage.
  virtual void SetEstimatedTimeLeft(const base::TimeDelta& time) = 0;

  // Shows screen curtains.
  virtual void ShowCurtain(bool enable) = 0;

  // Shows label for "Preparing updates" state.
  virtual void ShowPreparingUpdatesInfo(bool visible) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
