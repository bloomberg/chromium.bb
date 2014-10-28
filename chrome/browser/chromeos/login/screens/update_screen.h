// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_

#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen_actor.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

class BaseScreenDelegate;
class ErrorScreen;
class ErrorScreensHistogramHelper;
class NetworkState;
class ScreenManager;

// Controller for the update screen. It does not depend on the specific
// implementation of the screen showing (Views of WebUI based), the dependency
// is moved to the UpdateScreenActor instead.
class UpdateScreen : public UpdateEngineClient::Observer,
                     public UpdateScreenActor::Delegate,
                     public BaseScreen,
                     public NetworkPortalDetector::Observer {
 public:
  UpdateScreen(BaseScreenDelegate* base_screen_delegate,
               UpdateScreenActor* actor,
               pairing_chromeos::HostPairingController* remora_controller);
  virtual ~UpdateScreen();

  static UpdateScreen* Get(ScreenManager* manager);

  // Overridden from BaseScreen.
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // UpdateScreenActor::Delegate implementation:
  virtual void CancelUpdate() override;
  virtual void OnActorDestroyed(UpdateScreenActor* actor) override;
  virtual void OnConnectToNetworkRequested() override;

  // Starts network check. Made virtual to simplify mocking.
  virtual void StartNetworkCheck();

  // Reboot check delay get/set, in seconds.
  int reboot_check_delay() const { return reboot_check_delay_; }
  void SetRebootCheckDelay(int seconds);

  // Returns true if this instance is still active (i.e. has not been deleted).
  static bool HasInstance(UpdateScreen* inst);

  void SetIgnoreIdleStatus(bool ignore_idle_status);

  enum ExitReason {
     REASON_UPDATE_CANCELED = 0,
     REASON_UPDATE_INIT_FAILED,
     REASON_UPDATE_NON_CRITICAL,
     REASON_UPDATE_ENDED
  };
  // Reports update results to the BaseScreenDelegate.
  virtual void ExitUpdate(ExitReason reason);

  // UpdateEngineClient::Observer implementation:
  virtual void UpdateStatusChanged(
      const UpdateEngineClient::Status& status) override;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestAPReselection);

  enum State {
    STATE_IDLE = 0,
    STATE_FIRST_PORTAL_CHECK,
    STATE_UPDATE,
    STATE_ERROR
  };

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

  // Send update status to host pairing controller.
  void SetHostPairingControllerStatus(
      pairing_chromeos::HostPairingController::UpdateStatus update_status);

  // Returns an instance of the error screen.
  ErrorScreen* GetErrorScreen();

  void StartUpdateCheck();
  void ShowErrorMessage();
  void HideErrorMessage();
  void UpdateErrorMessage(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status);
  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer<UpdateScreen> reboot_timer_;

  // Returns a static InstanceSet.
  typedef std::set<UpdateScreen*> InstanceSet;
  static InstanceSet& GetInstanceSet();

  // Current state of the update screen.
  State state_;

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

  // Used to track updates over Bluetooth.
  pairing_chromeos::HostPairingController* remora_controller_;

  // Time of the first notification from the downloading stage.
  base::Time download_start_time_;
  double download_start_progress_;

  // Time of the last notification from the downloading stage.
  base::Time download_last_time_;
  double download_last_progress_;

  bool is_download_average_speed_computed_;
  double download_average_speed_;

  // True if there was no notification from NetworkPortalDetector
  // about state for the default network.
  bool is_first_detection_notification_;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_;

  scoped_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::WeakPtrFactory<UpdateScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
