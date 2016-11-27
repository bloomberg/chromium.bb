// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

class ArcAppLauncher;
class Profile;

namespace ash {
class ShelfDelegate;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace arc {

class ArcAndroidManagementChecker;
class ArcAuthCodeFetcher;
class ArcAuthContext;
class ArcOptInPreferenceHandler;
enum class ProvisioningResult : int;

// This class proxies the request from the client to fetch an auth code from
// LSO. It lives on the UI thread.
class ArcSessionManager : public ArcService,
                          public ArcBridgeService::Observer,
                          public ArcSupportHost::Observer,
                          public ArcOptInPreferenceHandlerObserver,
                          public sync_preferences::PrefServiceSyncableObserver,
                          public sync_preferences::SyncedPrefObserver {
 public:
  // Represents each State of ARC session.
  // NOT_INITIALIZED: represents the state that the Profile is not yet ready
  //   so that this service is not yet initialized, or Chrome is being shut
  //   down so that this is destroyed.
  // STOPPED: ARC session is not running, or being terminated.
  // SHOWING_TERMS_OF_SERVICE: "Terms Of Service" page is shown on ARC support
  //   Chrome app.
  // CHECKING_ANDROID_MANAGEMENT: Checking Android management status. Note that
  //   the status is checked for each ARC session starting, but this is the
  //   state only for the first boot case (= opt-in case). The second time and
  //   later the management check is running in parallel with ARC session
  //   starting, and in such a case, State is ACTIVE, instead.
  // FETCHING_CODE: Fetching an auth token. Similar to
  //   CHECKING_ANDROID_MANAGEMENT case, this is only for the first boot case.
  //   In re-auth flow (fetching an auth token while ARC is running), the
  //   State should be ACTIVE.
  //   TODO(hidehiko): Migrate into re-auth flow, then remove this state.
  // ACTIVE: ARC is running.
  //
  // State transition should be as follows:
  //
  // NOT_INITIALIZED -> STOPPED: when the primary Profile gets ready.
  // ...(any)... -> NOT_INITIALIZED: when the Chrome is being shutdown.
  // ...(any)... -> STOPPED: on error.
  //
  // In the first boot case (no OOBE case):
  //   STOPPED -> SHOWING_TERMS_OF_SERVICE: when arc.enabled preference is set.
  //   SHOWING_TERMS_OF_SERVICE -> CHECKING_ANDROID_MANAGEMENT: when a user
  //     agree with "Terms Of Service"
  //   CHECKING_ANDROID_MANAGEMENT -> FETCHING_CODE: when Android management
  //     check passes.
  //   FETCHING_CODE -> ACTIVE: when the auth token is successfully fetched.
  //
  // In the first boot case (OOBE case):
  //   STOPPED -> FETCHING_CODE: When arc.enabled preference is set.
  //   FETCHING_CODE -> ACTIVE: when the auth token is successfully fetched.
  //
  // In the second (or later) boot case:
  //   STOPPED -> ACTIVE: when arc.enabled preference is checked that it is
  //     true. Practically, this is when the primary Profile gets ready.
  enum class State {
    NOT_INITIALIZED,
    STOPPED,
    SHOWING_TERMS_OF_SERVICE,
    CHECKING_ANDROID_MANAGEMENT,
    ACTIVE,
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called to notify that ARC bridge is shut down.
    virtual void OnShutdownBridge() {}

    // Called to notify that ARC enabled state has been updated.
    virtual void OnOptInEnabled(bool enabled) {}

    // Called to notify that ARC has been initialized successfully.
    virtual void OnInitialStart() {}
  };

  explicit ArcSessionManager(ArcBridgeService* bridge_service);
  ~ArcSessionManager() override;

  static ArcSessionManager* Get();

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void DisableUIForTesting();
  static void SetShelfDelegateForTesting(ash::ShelfDelegate* shelf_delegate);

  // Checks if OptIn verification was disabled by switch in command line.
  static bool IsOptInVerificationDisabled();

  static void EnableCheckAndroidManagementForTesting();

  // Returns true if Arc is allowed to run for the given profile.
  static bool IsAllowedForProfile(const Profile* profile);

  // Returns true if ARC should run under Kiosk mode.
  static bool IsArcKioskMode();

  // Returns true if Arc is allowed to run for the current session.
  bool IsAllowed() const;

  void OnPrimaryUserProfilePrepared(Profile* profile);
  void Shutdown();

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }

  State state() const { return state_; }

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ArcBridgeService::Observer:
  void OnBridgeStopped(ArcBridgeService::StopReason reason) override;

  // Called from Arc support platform app when user cancels signing.
  void CancelAuthCode();

  bool IsArcManaged() const;
  bool IsArcEnabled() const;

  // This requires Arc to be allowed (|IsAllowed|)for current profile.
  void EnableArc();
  void DisableArc();

  // Called from the Chrome OS metrics provider to record Arc.State
  // periodically.
  void RecordArcState();

  // sync_preferences::PrefServiceSyncableObserver
  void OnIsSyncingChanged() override;

  // sync_preferences::SyncedPrefObserver
  void OnSyncedPrefChanged(const std::string& path, bool from_sync) override;

  // ArcSupportHost::Observer:
  void OnWindowClosed() override;
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnRetryClicked() override;
  void OnSendFeedbackClicked() override;

  // ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  // Stops ARC without changing ArcEnabled preference.
  void StopArc();

  // StopArc(), then EnableArc(). Between them data clear may happens.
  // This is a special method to support enterprise device lost case.
  // This can be called only when ARC is running.
  void StopAndEnableArc();

  // Removes the data if ARC is stopped. Otherwise, queue to remove the data
  // on ARC is stopped.
  void RemoveArcData();

  ArcSupportHost* support_host() { return support_host_.get(); }

  // TODO(hidehiko): Get rid of the getter by migration between ArcAuthContext
  // and ArcAuthCodeFetcher.
  ArcAuthContext* auth_context() { return context_.get(); }

  void StartArc();

  void OnProvisioningFinished(ProvisioningResult result);

 private:
  // TODO(hidehiko): move UI methods/fields to ArcSupportHost.
  void SetState(State state);
  void ShutdownBridge();
  void OnOptInPreferenceChanged();
  void StartUI();
  void OnAndroidManagementPassed();
  void OnArcDataRemoved(bool success);
  void OnArcSignInTimeout();
  void FetchAuthCode();
  void PrepareContextForAuthCodeRequest();

  void StartArcAndroidManagementCheck();

  // Called when the Android management check is done in opt-in flow or
  // re-auth flow.
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Called when the background Android management check is done. It is
  // triggered when the second or later ARC boot timing.
  void OnBackgroundAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  State state_ = State::NOT_INITIALIZED;
  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
  bool clear_required_ = false;
  bool reenable_arc_ = false;
  base::OneShotTimer arc_sign_in_timer_;

  // Temporarily keeps the ArcSupportHost instance.
  // This should be moved to ArcSessionManager when the refactoring is
  // done.
  std::unique_ptr<ArcSupportHost> support_host_;
  // Handles preferences and metrics mode.
  std::unique_ptr<ArcOptInPreferenceHandler> preference_handler_;

  std::unique_ptr<ArcAuthContext> context_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  base::Time sign_in_time_;

  base::WeakPtrFactory<ArcSessionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManager);
};

// Outputs the stringified |state| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
