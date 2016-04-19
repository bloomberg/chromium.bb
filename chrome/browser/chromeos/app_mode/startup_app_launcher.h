// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/extensions/install_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

namespace chromeos {

// Launches the app at startup. The flow roughly looks like this:
// First call Initialize():
// - Attempts to load oauth token file. Stores the loaded tokens in
//   |auth_params_|.
// - Initialize token service and inject |auth_params_| if needed.
// - Initialize network if app is not installed or not offline_enabled.
// - If network is online, install or update the app as needed.
// - After the app is installed/updated, launch it and finish the flow;
// Report OnLauncherInitialized() or OnLaunchFailed() to observers:
// - If all goes good, launches the app and finish the flow;
class StartupAppLauncher : public base::SupportsWeakPtr<StartupAppLauncher>,
                           public OAuth2TokenService::Observer,
                           public extensions::InstallObserver,
                           public KioskAppManagerObserver,
                           public content::NotificationObserver {
 public:
  class Delegate {
   public:
    // Invoked to perform actual network initialization work. Note the app
    // launch flow is paused until ContinueWithNetworkReady is called.
    virtual void InitializeNetwork() = 0;

    // Returns true if Internet is online.
    virtual bool IsNetworkReady() = 0;

    virtual void OnLoadingOAuthFile() = 0;
    virtual void OnInitializingTokenService() = 0;
    virtual void OnInstallingApp() = 0;
    virtual void OnReadyToLaunch() = 0;
    virtual void OnLaunchSucceeded() = 0;
    virtual void OnLaunchFailed(KioskAppLaunchError::Error error) = 0;
    virtual bool IsShowingNetworkConfigScreen() = 0;

   protected:
    virtual ~Delegate() {}
  };

  StartupAppLauncher(Profile* profile,
                     const std::string& app_id,
                     bool diagnostic_mode,
                     Delegate* delegate);

  ~StartupAppLauncher() override;

  // Prepares the environment for an app launch.
  void Initialize();

  // Continues the initialization after network is ready.
  void ContinueWithNetworkReady();

  // Launches the app after the initialization is successful.
  void LaunchApp();

  // Restarts launcher;
  void RestartLauncher();

 private:
  // OAuth parameters from /home/chronos/kiosk_auth file.
  struct KioskOAuthParams {
    std::string refresh_token;
    std::string client_id;
    std::string client_secret;
  };

  void OnLaunchSuccess();
  void OnLaunchFailure(KioskAppLaunchError::Error error);

  void BeginInstall();
  void OnReadyToLaunch();
  void MaybeUpdateAppData();

  void InitializeTokenService();
  void MaybeInitializeNetwork();
  void MaybeInstallSecondaryApps();
  void MaybeLaunchApp();

  void MaybeCheckExtensionUpdate();
  void OnExtensionUpdateCheckFinished();

  void StartLoadingOAuthFile();
  static void LoadOAuthFileOnBlockingPool(KioskOAuthParams* auth_params);
  void OnOAuthFileLoaded(KioskOAuthParams* auth_params);

  void OnKioskAppDataLoadStatusChanged(const std::string& app_id);

  // Returns true if any secondary app is pending.
  bool IsAnySecondaryAppPending() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  // Returns true if secondary apps are declared in manifest.
  bool HasSecondaryApps() const;

  // Returns true if the primary app has a pending update.
  bool PrimaryAppHasPendingUpdate() const;

  // Returns true if the app with |id| failed, and it is the primary or one of
  // the secondary apps.
  bool DidPrimaryOrSecondaryAppFailedToInstall(bool success,
                                               const std::string& id) const;

  const extensions::Extension* GetPrimaryAppExtension() const;

  // OAuth2TokenService::Observer overrides.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  // extensions::InstallObserver overrides.
  void OnFinishCrxInstall(const std::string& extension_id,
                          bool success) override;

  // KioskAppManagerObserver overrides.
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  Profile* profile_;
  const std::string app_id_;
  const bool diagnostic_mode_;
  Delegate* delegate_;
  bool network_ready_handled_ = false;
  int launch_attempt_ = 0;
  bool ready_to_launch_ = false;
  bool wait_for_crx_update_ = false;
  bool secondary_apps_installed_ = false;
  bool extension_update_found_ = false;

  KioskOAuthParams auth_params_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StartupAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_STARTUP_APP_LAUNCHER_H_
