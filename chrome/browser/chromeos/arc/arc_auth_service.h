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
    DISABLE,        // ARC is not allowed to run (default).
    FETCHING_CODE,  // ARC is allowed, receiving auth_2 code.
    NO_CODE,        // ARC is allowed, auth_2 code was not received.
    ENABLE,         // ARC is allowed, auth_2 code was received.
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called whenever Opt-In state of the ARC has been changed.
    virtual void OnOptInChanged(State state) {}

    // Called to notify that OptIn UI needs to be closed.
    virtual void OnOptInUINeedToClose() {}
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

  // Overrides AuthHost.  For security reason this code can be used only
  // once and exists for specific period of time.
  void GetAuthCodeDeprecated(
      const GetAuthCodeDeprecatedCallback& callback) override;
  void GetAuthCode(const GetAuthCodeCallback& callback) override;

  // May be called internally as response to on Arc OptIn preference change
  // or externally from Arc support platform app.
  void FetchAuthCode();

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

 private:
  void SetAuthCodeAndStartArc(const std::string& auth_code);
  void ShowUI();
  void CloseUI();
  void SetState(State state);
  void ShutdownBridgeAndCloseUI();
  void OnOptInPreferenceChanged();

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;
  // Owned by content::BrowserContent. Used to isolate cookies for auth server
  // communication and shared with Arc OptIn UI platform app.
  content::StoragePartition* storage_partition_ = nullptr;

  // Registrar used to monitor ARC opt-in state.
  PrefChangeRegistrar pref_change_registrar_;

  mojo::Binding<AuthHost> binding_;
  base::ThreadChecker thread_checker_;
  State state_ = State::DISABLE;
  base::ObserverList<Observer> observer_list_;
  scoped_ptr<ArcAuthFetcher> auth_fetcher_;
  scoped_ptr<GaiaAuthFetcher> merger_fetcher_;
  scoped_ptr<UbertokenFetcher> ubertoken_fethcher_;
  std::string auth_code_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

std::ostream& operator<<(std::ostream& os, const ArcAuthService::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_SERVICE_H_
