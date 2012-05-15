// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

class ScreenObserver;

// Controller for the update screen. It does not depend on the specific
// implementation of the screen showing (Views of WebUI based), the dependency
// is moved to the UpdateScreenActor instead.
class UpdateScreen: public UpdateEngineClient::Observer,
                    public UpdateScreenActor::Delegate,
                    public WizardScreen {
 public:
  UpdateScreen(ScreenObserver* screen_observer, UpdateScreenActor* actor);
  virtual ~UpdateScreen();

  // Overridden from WizardScreen.
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;

  // UpdateScreenActor::Delegate implementation:
  virtual void CancelUpdate() OVERRIDE;
  virtual void OnActorDestroyed(UpdateScreenActor* actor) OVERRIDE;

  // Checks for updates and performs an update if needed. Made virtual to
  // simplify mocking.
  virtual void StartUpdate();

  // Reboot check delay get/set, in seconds.
  int reboot_check_delay() const { return reboot_check_delay_; }
  void SetRebootCheckDelay(int seconds);

  // Returns true if this instance is still active (i.e. has not been deleted).
  static bool HasInstance(UpdateScreen* inst);

  void SetIgnoreIdleStatus(bool ignore_idle_status);

  enum ExitReason {
     REASON_UPDATE_CANCELED,
     REASON_UPDATE_INIT_FAILED,
     REASON_UPDATE_NON_CRITICAL,
     REASON_UPDATE_ENDED
  };
  // Reports update results to the ScreenObserver.
  virtual void ExitUpdate(ExitReason reason);

  // UpdateEngineClient::Observer implementation:
  virtual void UpdateStatusChanged(
      const UpdateEngineClient::Status& status) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);

  // Updates downloading stats (remaining time and downloading
  // progress) on the AU screen.
  void UpdateDownloadingStats(const UpdateEngineClient::Status& status);

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
  // Note, this is false by default.
  bool is_ignore_update_deadlines_;
  // Whether the update screen is shown.
  bool is_shown_;
  // Ignore fist IDLE status that is sent before update screen initiated check.
  bool ignore_idle_status_;

  // Keeps actor which is delegated with all showing operations.
  UpdateScreenActor* actor_;

  // Time of the first notification from the downloading stage.
  base::Time download_start_time_;
  double download_start_progress_;

  // Time of the last notification from the downloading stage.
  base::Time download_last_time_;
  double download_last_progress_;

  bool is_download_average_speed_computed_;
  double download_average_speed_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
