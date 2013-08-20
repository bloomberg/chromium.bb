// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "net/base/network_change_notifier.h"

class Profile;

namespace extensions {
class WebstoreStandaloneInstaller;
}

namespace chromeos {

// Launches the app at startup. The flow roughly looks like this:
// - Checks if the app is installed in user profile (aka app profile);
// - If the app is installed, launch it and finish the flow;
// - If not installed, prepare to start install by checking network online
//   state;
// - If network gets online in time, start to install the app from web store;
// - If all goes good, launches the app and finish the flow;
class StartupAppLauncher
    : public base::SupportsWeakPtr<StartupAppLauncher>,
      public OAuth2TokenService::Observer,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  class Observer {
   public:
    virtual void OnLoadingOAuthFile() = 0;
    virtual void OnInitializingTokenService() = 0;
    virtual void OnInitializingNetwork() = 0;
    virtual void OnNetworkWaitTimedout() = 0;
    virtual void OnInstallingApp() = 0;
    virtual void OnLaunchSucceeded() = 0;
    virtual void OnLaunchFailed(KioskAppLaunchError::Error error) = 0;

   protected:
    virtual ~Observer() {}
  };

  StartupAppLauncher(Profile* profile, const std::string& app_id);

  virtual ~StartupAppLauncher();

  // Starts app launcher. If |skip_auth_setup| is set, we will skip
  // TokenService initialization.
  void Start();

  // Add and remove observers for app launch procedure.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // OAuth parameters from /home/chronos/kiosk_auth file.
  struct KioskOAuthParams {
    std::string refresh_token;
    std::string client_id;
    std::string client_secret;
  };

  void OnLaunchSuccess();
  void OnLaunchFailure(KioskAppLaunchError::Error error);

  void Launch();

  void BeginInstall();
  void InstallCallback(bool success, const std::string& error);

  void InitializeTokenService();
  void InitializeNetwork();

  void OnNetworkWaitTimedout();

  void StartLoadingOAuthFile();
  static void LoadOAuthFileOnBlockingPool(KioskOAuthParams* auth_params);
  void OnOAuthFileLoaded(KioskOAuthParams* auth_params);

  // OAuth2TokenService::Observer overrides.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides:
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  Profile* profile_;
  const std::string app_id_;
  ObserverList<Observer> observer_list_;

  scoped_refptr<extensions::WebstoreStandaloneInstaller> installer_;
  base::OneShotTimer<StartupAppLauncher> network_wait_timer_;
  KioskOAuthParams auth_params_;

  DISALLOW_COPY_AND_ASSIGN(StartupAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
