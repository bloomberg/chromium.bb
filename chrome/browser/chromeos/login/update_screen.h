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

  // Overridden from ViewScreen.
  virtual void Show();

  enum ExitReason {
     REASON_UPDATE_CANCELED,
     REASON_UPDATE_INIT_FAILED,
     REASON_UPDATE_NON_CRITICAL,
     REASON_UPDATE_ENDED
  };

  // Reports update results to the ScreenObserver.
  virtual void ExitUpdate(ExitReason reason);

  // Reboot check delay get/set, in seconds.
  int reboot_check_delay() const { return reboot_check_delay_; }
  void SetRebootCheckDelay(int seconds);

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Set flag to treat all updates as critical (for test purpose mainly).
  // Default value is false.
  void SetAllUpdatesCritical(bool is_critical);

 private:
  // Timer notification handlers.
  void OnWaitForRebootTimeElapsed();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer<UpdateScreen> reboot_timer_;

  // True if in the process of checking for update.
  bool checking_for_update_;

  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  int reboot_check_delay_;

  // Flag that is used to detect when update download has just started.
  bool is_downloading_update_;

  // Is all updates critical? If true, update deadlines are ignored.
  bool is_all_updates_critical_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
