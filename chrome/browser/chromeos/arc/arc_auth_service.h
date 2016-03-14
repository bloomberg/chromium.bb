// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_

#include <ostream>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/auth/arc_auth_fetcher.h"
#include "components/arc/common/auth.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "mojo/public/cpp/bindings/binding.h"

class GaiaAuthFetcher;
class Profile;

namespace content {
class StoragePartition;
}

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace arc {

// This class proxies the request from the client to fetch an auth code from
// LSO.
class ArcAuthService : public ArcService,
                       public AuthHost,
                       public ArcBridgeService::Observer,
                       public ArcAuthFetcher::Delegate,
                       public UbertokenConsumer,
                       public GaiaAuthConsumer,
                       public syncable_prefs::PrefServiceSyncableObserver {
 public:
  enum class State {
    STOPPED,        // ARC is not running.
    FETCHING_CODE,  // ARC may be running or not. Auth code is fetching.
    ACTIVE,         // ARC is running.
  };

  enum class UIPage {
    NO_PAGE,         // Hide everything.
    START,           // Initial start page.
    LSO_PROGRESS,    // LSO loading progress page.
    LSO,             // LSO page to enter user's credentials.
    START_PROGRESS,  // Arc starting progress page.
    ERROR,           // Arc start error page.
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
  };

  explicit ArcAuthService(ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

  static ArcAuthService* Get();

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void DisableUIForTesting();

  // Checks if OptIn verification was disabled by switch in command line.
  static bool IsOptInVerificationDisabled();

  void OnPrimaryUserProfilePrepared(Profile* profile);
  void Shutdown();

  State state() const { return state_; }

  std::string GetAndResetAuthCode();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ArcBridgeService::Observer:
  void OnAuthInstanceReady() override;

  // AuthHost:
  // For security reason this code can be used only once and exists for specific
  // period of time.
  void GetAuthCodeDeprecated(
      const GetAuthCodeDeprecatedCallback& callback) override;
  void GetAuthCode(const GetAuthCodeCallback& callback) override;
  void OnSignInComplete() override;
  void OnSignInFailed(arc::ArcSignInFailureReason reason) override;

  // Called from Arc support platform app when user clicks 'Get Started'.
  void GetStarted();

  // Called from Arc support platform app to check auth code.
  void CheckAuthCode();

  // Called from Arc support platform app when user cancels signing.
  void CancelAuthCode();

  void EnableArc();
  void DisableArc();

  // ArcAuthFetcher::Delegate:
  void OnAuthCodeFetched(const std::string& auth_code) override;
  void OnAuthCodeNeedUI() override;
  void OnAuthCodeFailed() override;

  // UbertokenConsumer:
  void OnUbertokenSuccess(const std::string& token) override;
  void OnUbertokenFailure(const GoogleServiceAuthError& error) override;

  // GaiaAuthConsumer:
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;

  // syncable_prefs::PrefServiceSyncableObserver
  void OnIsSyncingChanged() override;

  // Returns current page that has to be shown in OptIn UI.
  UIPage ui_page() const { return ui_page_; }
  // Returns current page status, relevant to the specific page.
  const base::string16& ui_page_status() { return ui_page_status_; }

 private:
  void StartArc();
  void SetAuthCodeAndStartArc(const std::string& auth_code);
  void PrepareContext();
  void ShowUI(UIPage page, const base::string16& status);
  void CloseUI();
  void SetUIPage(UIPage page, const base::string16& status);
  void SetState(State state);
  void ShutdownBridge();
  void ShutdownBridgeAndCloseUI();
  void ShutdownBridgeAndShowUI(UIPage page, const base::string16& status);
  void OnOptInPreferenceChanged();
  void FetchAuthCode();

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;
  // Owned by content::BrowserContent. Used to isolate cookies for auth server
  // communication and shared with Arc OptIn UI platform app.
  content::StoragePartition* storage_partition_ = nullptr;

  // Registrar used to monitor ARC opt-in state.
  PrefChangeRegistrar pref_change_registrar_;

  mojo::Binding<AuthHost> binding_;
  base::ThreadChecker thread_checker_;
  State state_ = State::STOPPED;
  base::ObserverList<Observer> observer_list_;
  scoped_ptr<ArcAuthFetcher> auth_fetcher_;
  scoped_ptr<GaiaAuthFetcher> merger_fetcher_;
  scoped_ptr<UbertokenFetcher> ubertoken_fethcher_;
  std::string auth_code_;
  GetAuthCodeCallback auth_callback_;
  bool initial_opt_in_ = false;
  bool context_prepared_ = false;
  UIPage ui_page_ = UIPage::NO_PAGE;
  base::string16 ui_page_status_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
