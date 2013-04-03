// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/events/event_handler.h"

class Profile;

namespace extensions {
class WebstoreStandaloneInstaller;
}

namespace chromeos {

// Launches the app at startup. The flow roughly looks like this:
// - Starts the app launch splash screen;
// - Checks if the app is installed in user profile (aka app profile);
// - If the app is installed, launch it and finish the flow;
// - If not installed, prepare to start install by checking network online
//   state;
// - If network gets online in time, start to install the app from web store;
// - If all goes good, launches the app and finish the flow;
// If anything goes wrong, it exits app mode and goes back to login screen.
class StartupAppLauncher
    : public base::SupportsWeakPtr<StartupAppLauncher>,
      public content::NotificationObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public ui::EventHandler {
 public:
  StartupAppLauncher(Profile* profile, const std::string& app_id);

  // Starts app launcher. If |skip_auth_setup| is set, we will skip
  // TokenService initialization.
  void Start();

 private:
  // OAuth parameters from /home/chronos/kiosk_auth file.
  struct KioskOAuthParams {
    std::string refresh_token;
    std::string client_id;
    std::string client_secret;
  };

  // Private dtor because this class manages its own lifetime.
  virtual ~StartupAppLauncher();

  void Cleanup();
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

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides:
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

  Profile* profile_;
  const std::string app_id_;

  int64 launch_splash_start_time_;

  scoped_refptr<extensions::WebstoreStandaloneInstaller> installer_;
  content::NotificationRegistrar registrar_;
  base::OneShotTimer<StartupAppLauncher> network_wait_timer_;
  KioskOAuthParams auth_params_;

  DISALLOW_COPY_AND_ASSIGN(StartupAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
