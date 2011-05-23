// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#pragma once

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

namespace chromeos {

// Forward declaration.
class UpdateScreenActor;

// Controller for the update screen. It does not depend on the specific
// implementation of the screen showing (Views of WebUI based), the dependency
// is moved to the UpdateScreenActor instead.
class UpdateScreen: public UpdateLibrary::Observer,
                    public WizardScreen {
 public:
  explicit UpdateScreen(WizardScreenDelegate* delegate);
  virtual ~UpdateScreen();

  // Overridden from WizardScreen.
  virtual void Show();
  virtual void Hide();
  virtual gfx::Size GetScreenSize() const;

  // Checks for updates and performs an update if needed. Made virtual to
  // simplify mocking.
  virtual void StartUpdate();

  // Force cancel update. Made virtual to simplify mocking.
  virtual void CancelUpdate();

  // Reboot check delay get/set, in seconds.
  int reboot_check_delay() const { return reboot_check_delay_; }
  void SetRebootCheckDelay(int seconds);

  // Returns true if this instance is still active (i.e. has not been deleted).
  static bool HasInstance(UpdateScreen* inst);

  enum ExitReason {
     REASON_UPDATE_CANCELED,
     REASON_UPDATE_INIT_FAILED,
     REASON_UPDATE_NON_CRITICAL,
     REASON_UPDATE_ENDED
  };
  // Reports update results to the ScreenObserver.
  virtual void ExitUpdate(ExitReason reason);

  // UpdateLibrary::Observer implementation:
  virtual void UpdateStatusChanged(UpdateLibrary* library);

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Timer notification handlers.
  void OnWaitForRebootTimeElapsed();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer<UpdateScreen> reboot_timer_;

  // Returns a static InstanceSet.
  typedef std::set<UpdateScreen*> InstanceSet;
  static InstanceSet& GetInstanceSet();

  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  int reboot_check_delay_;

  // True if in the process of checking for update.
  bool is_checking_for_update_;
  // Flag that is used to detect when update download has just started.
  bool is_downloading_update_;
  // If true, update deadlines are ignored.
  // Note, this is true by default. See "http://crosbug.com/10068".
  bool is_ignore_update_deadlines_;
  // Whether the update screen is shown.
  bool is_shown_;

  // Keeps actor which is delegated with all showing operations.
  scoped_ptr<UpdateScreenActor> actor_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
