// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
#pragma once

namespace chromeos {

class UpdateScreenActor {
 public:
  virtual ~UpdateScreenActor() {}

  // Shows the screen.
  virtual void Show() = 0;

  // Hides the screen.
  virtual void Hide() = 0;

  // Shows manual reboot info message.
  virtual void ShowManualRebootInfo() = 0;

  // Sets current progress in percents.
  virtual void SetProgress(int progress) = 0;

  // Shows screen curtains.
  virtual void ShowCurtain(bool enable) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_ACTOR_H_
