// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

class UpdateController {
 public:
  // Starts update.
  virtual void StartUpdate() = 0;
  // Cancels pending update without error.
  virtual void CancelUpdate() = 0;
};

class UpdateScreen: public DefaultViewScreen<chromeos::UpdateView>,
                    public UpdateLibrary::Observer,
                    public UpdateController {
 public:
  explicit UpdateScreen(WizardScreenDelegate* delegate);
  virtual ~UpdateScreen();

  // UpdateLibrary::Observer implementation:
  virtual void UpdateStatusChanged(UpdateLibrary* library);

  // Overridden from UpdateController:
  virtual void StartUpdate();
  virtual void CancelUpdate();

  // Reports update results to the ScreenObserver.
  virtual void ExitUpdate();

  // Returns true if minimal update time has elapsed.
  virtual bool MinimalUpdateTimeElapsed();

  // Minimal update time get/set, in seconds.
  int minimal_update_time() const { return minimal_update_time_; }
  void SetMinimalUpdateTime(int seconds);

  // Reboot check delay get/set, in seconds.
  int reboot_check_delay() const { return reboot_check_delay_; }
  void SetRebootCheckDelay(int seconds);

 private:
  // Timer notification handlers.
  void OnMinimalUpdateTimeElapsed();
  void OnWaitForRebootTimeElapsed();

  // Timer for the minimal interval when update screen is shown.
  base::OneShotTimer<UpdateScreen> minimal_update_time_timer_;

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer<UpdateScreen> reboot_timer_;

  // True if should proceed with OOBE once timer is elapsed.
  bool proceed_with_oobe_;

  // True if in the process of checking for update.
  bool checking_for_update_;

  // Minimal update delay in seconds, for a user to notice
  // check for update is taking place.
  int minimal_update_time_;

  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen durin this interval, ask user to
  // reboot device manually.
  int reboot_check_delay_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
