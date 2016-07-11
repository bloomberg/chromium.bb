// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/arc/arc_android_management_checker_delegate.h"
#include "chrome/browser/chromeos/arc/arc_auth_context_delegate.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/auth.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"
#include "components/syncable_prefs/synced_pref_observer.h"
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
class ArcAuthContext;

// This class proxies the request from the client to fetch an auth code from
// LSO. It lives on the UI thread.
class ArcAuthService : public ArcService,
                       public mojom::AuthHost,
                       public ArcBridgeService::Observer,
                       public ArcAndroidManagementCheckerDelegate,
                       public ArcAuthContextDelegate,
                       public syncable_prefs::PrefServiceSyncableObserver,
                       public syncable_prefs::SyncedPrefObserver {
 public:
  enum class State {
    NOT_INITIALIZED,  // Service is not initialized.
    STOPPED,          // ARC is not running.
    FETCHING_CODE,    // ARC may be running or not. Auth code is fetching.
    ACTIVE,           // ARC is running.
  };

  enum class UIPage {
    NO_PAGE,              // Hide everything.
    TERMS_PROGRESS,       // Terms loading progress page.
    TERMS,                // Terms content page.
    LSO_PROGRESS,         // LSO loading progress page.
    LSO,                  // LSO page to enter user's credentials.
    START_PROGRESS,       // Arc starting progress page.
    ERROR,                // Arc start error page.
    ERROR_WITH_FEEDBACK,  // Arc start error page, plus feedback button.
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called whenever Opt-In state of the ARC has been changed.
    virtual void OnOptInChanged(State state) {}

    // Called to notify that OptIn UI needs to be closed.
    virtual void OnOptInUIClose() {}

    // Called to notify that OptIn UI needs to show specific page.
    virtual void OnOptInUIShowPage(UIPage page, const base::string16& status) {}

    // Called to notify that ARC bridge is shut down.
    virtual void OnShutdownBridge() {}

    // Called to notify that ARC enabled state has been updated.
    virtual void OnOptInEnabled(bool enabled) {}
  };

  explicit ArcAuthService(ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

  static ArcAuthService* Get();

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void DisableUIForTesting();
  static void SetShelfDelegateForTesting(ash::ShelfDelegate* shelf_delegate);

  // Checks if OptIn verification was disabled by switch in command line.
  static bool IsOptInVerificationDisabled();

  static void EnableCheckAndroidManagementForTesting();

  // Returns true if Arc is allowed to run for the given profile.
  static bool IsAllowedForProfile(const Profile* profile);

  // Returns true if Arc is allowed to run for the current session.
  bool IsAllowed() const;

  void OnPrimaryUserProfilePrepared(Profile* profile);
  void Shutdown();

  Profile* profile() { return profile_; }

  State state() const { return state_; }

  std::string GetAndResetAuthCode();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ArcBridgeService::Observer:
  void OnAuthInstanceReady() override;
  void OnBridgeStopped() override;

  // AuthHost:
  // For security reason this code can be used only once and exists for specific
  // period of time.
  void GetAuthCodeDeprecated(
      const GetAuthCodeDeprecatedCallback& callback) override;
  void GetAuthCode(const GetAuthCodeCallback& callback) override;
  void OnSignInComplete() override;
  void OnSignInFailed(arc::mojom::ArcSignInFailureReason reason) override;
  // Callback is called with a bool that indicates the management status of the
  // user.
  void GetIsAccountManaged(
      const GetIsAccountManagedCallback& callback) override;

  // Called from Arc support platform app to start LSO.
  void StartLso();

  // Called from Arc support platform app to set auth code and start arc.
  void SetAuthCodeAndStartArc(const std::string& auth_code);

  // Called from Arc support platform app when user cancels signing.
  void CancelAuthCode();

  bool IsArcManaged() const;
  bool IsArcEnabled() const;
  void EnableArc();
  void DisableArc();

  // syncable_prefs::PrefServiceSyncableObserver
  void OnIsSyncingChanged() override;

  // syncable_prefs::SyncedPrefObserver
  void OnSyncedPrefChanged(const std::string& path, bool from_sync) override;

  // ArcAuthContextDelegate:
  void OnContextReady() override;
  void OnPrepareContextFailed() override;

  // ArcAndroidManagementCheckerDelegate:
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result) override;

  // Returns current page that has to be shown in OptIn UI.
  UIPage ui_page() const { return ui_page_; }

  // Returns current page status, relevant to the specific page.
  const base::string16& ui_page_status() { return ui_page_status_; }

 private:
  void StartArc();
  void ShowUI(UIPage page, const base::string16& status);
  void CloseUI();
  void SetUIPage(UIPage page, const base::string16& status);
  void SetState(State state);
  void ShutdownBridge();
  void ShutdownBridgeAndCloseUI();
  void ShutdownBridgeAndShowUI(UIPage page, const base::string16& status);
  void OnOptInPreferenceChanged();
  void StartUI();
  void StartAndroidManagementClient();
  void CheckAndroidManagement(bool background_mode);
  void StartArcIfSignedIn();
  void StopArc();

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  mojo::Binding<AuthHost> binding_;
  State state_ = State::NOT_INITIALIZED;
  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
  std::string auth_code_;
  GetAuthCodeCallback auth_callback_;
  bool initial_opt_in_ = false;
  bool disable_arc_from_ui_ = false;
  UIPage ui_page_ = UIPage::NO_PAGE;
  base::string16 ui_page_status_;
  bool clear_required_ = false;
  bool waiting_for_reply_ = false;

  std::unique_ptr<ArcAuthContext> context_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  base::Time sign_in_time_;

  base::WeakPtrFactory<ArcAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
