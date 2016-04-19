// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class RefCountedString;
}

namespace extensions {
class Extension;
class ExternalLoader;
}

namespace chromeos {

class AppSession;
class KioskAppData;
class KioskAppExternalLoader;
class KioskAppManagerObserver;
class KioskExternalUpdater;
class OwnerSettingsServiceChromeOS;

// KioskAppManager manages cached app data.
class KioskAppManager : public KioskAppDataDelegate,
                        public ExternalCache::Delegate {
 public:
  enum ConsumerKioskAutoLaunchStatus {
    // Consumer kiosk mode auto-launch feature can be enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
    // Consumer kiosk auto-launch feature is enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED,
    // Consumer kiosk mode auto-launch feature is disabled and cannot any longer
    // be enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED,
  };

  typedef base::Callback<void(bool success)> EnableKioskAutoLaunchCallback;
  typedef base::Callback<void(ConsumerKioskAutoLaunchStatus status)>
      GetConsumerKioskAutoLaunchStatusCallback;

  // Struct to hold app info returned from GetApps() call.
  struct App {
    App(const KioskAppData& data,
        bool is_extension_pending,
        bool was_auto_launched_with_zero_delay);
    App();
    App(const App& other);
    ~App();

    std::string app_id;
    AccountId account_id;
    std::string name;
    gfx::ImageSkia icon;
    std::string required_platform_version;
    bool is_loading;
    bool was_auto_launched_with_zero_delay;
  };
  typedef std::vector<App> Apps;

  // Name of a dictionary that holds kiosk app info in Local State.
  // Sample layout:
  //   "kiosk": {
  //     "auto_login_enabled": true  //
  //   }
  static const char kKioskDictionaryName[];
  static const char kKeyApps[];
  static const char kKeyAutoLoginState[];

  // Sub directory under DIR_USER_DATA to store cached icon files.
  static const char kIconCacheDir[];

  // Sub directory under DIR_USER_DATA to store cached crx files.
  static const char kCrxCacheDir[];

  // Sub directory under DIR_USER_DATA to store unpacked crx file for validating
  // its signature.
  static const char kCrxUnpackDir[];

  // Gets the KioskAppManager instance, which is lazily created on first call..
  static KioskAppManager* Get();

  // Prepares for shutdown and calls CleanUp() if needed.
  static void Shutdown();

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes cryptohomes which could not be removed during the previous session.
  static void RemoveObsoleteCryptohomes();

  // Initiates reading of consumer kiosk mode auto-launch status.
  void GetConsumerKioskAutoLaunchStatus(
      const GetConsumerKioskAutoLaunchStatusCallback& callback);

  // Enables consumer kiosk mode app auto-launch feature. Upon completion,
  // |callback| will be invoked with outcome of this operation.
  void EnableConsumerKioskAutoLaunch(
      const EnableKioskAutoLaunchCallback& callback);

  // Returns true if this device is consumer kiosk auto launch enabled.
  bool IsConsumerKioskDeviceWithAutoLaunch();

  // Returns auto launcher app id or an empty string if there is none.
  std::string GetAutoLaunchApp() const;

  // Sets |app_id| as the app to auto launch at start up.
  void SetAutoLaunchApp(const std::string& app_id,
                        OwnerSettingsServiceChromeOS* service);

  // Returns true if there is a pending auto-launch request.
  bool IsAutoLaunchRequested() const;

  // Returns true if owner/policy enabled auto launch.
  bool IsAutoLaunchEnabled() const;

  // Enable auto launch setter.
  void SetEnableAutoLaunch(bool value);

  // Returns the cached required platform version of the auto launch with
  // zero delay kiosk app.
  std::string GetAutoLaunchAppRequiredPlatformVersion() const;

  // Adds/removes a kiosk app by id. When removed, all locally cached data
  // will be removed as well.
  void AddApp(const std::string& app_id, OwnerSettingsServiceChromeOS* service);
  void RemoveApp(const std::string& app_id,
                 OwnerSettingsServiceChromeOS* service);

  // Gets info of all apps that have no meta data load error.
  void GetApps(Apps* apps) const;

  // Gets app data for the given app id. Returns true if |app_id| is known and
  // |app| is populated. Otherwise, return false.
  bool GetApp(const std::string& app_id, App* app) const;

  // Gets whether the bailout shortcut is disabled.
  bool GetDisableBailoutShortcut() const;

  // Clears locally cached app data.
  void ClearAppData(const std::string& app_id);

  // Updates app data from the |app| in |profile|. |app| is provided to cover
  // the case of app update case where |app| is the new version and is not
  // finished installing (e.g. because old version is still running). Otherwise,
  // |app| could be NULL and the current installed app in |profile| will be
  // used.
  void UpdateAppDataFromProfile(const std::string& app_id,
                                Profile* profile,
                                const extensions::Extension* app);

  void RetryFailedAppDataFetch();

  // Returns true if the app is found in cache.
  bool HasCachedCrx(const std::string& app_id) const;

  // Gets the path and version of the cached crx with |app_id|.
  // Returns true if the app is found in cache.
  bool GetCachedCrx(const std::string& app_id,
                    base::FilePath* file_path,
                    std::string* version) const;

  void AddObserver(KioskAppManagerObserver* observer);
  void RemoveObserver(KioskAppManagerObserver* observer);

  // Creates extensions::ExternalLoader for installing the primary kiosk app
  // during its first time launch.
  extensions::ExternalLoader* CreateExternalLoader();

  // Creates extensions::ExternalLoader for installing secondary kiosk apps
  // before launching the primary app for the first time.
  extensions::ExternalLoader* CreateSecondaryAppExternalLoader();

  // Installs kiosk app with |id| from cache.
  void InstallFromCache(const std::string& id);

  // Installs the secondary apps listed in |ids|.
  void InstallSecondaryApps(const std::vector<std::string>& ids);

  void UpdateExternalCache();

  // Monitors kiosk external update from usb stick.
  void MonitorKioskExternalUpdate();

  // Invoked when kiosk app cache has been updated.
  void OnKioskAppCacheUpdated(const std::string& app_id);

  // Invoked when kiosk app updating from usb stick has been completed.
  // |success| indicates if all the updates are completed successfully.
  void OnKioskAppExternalUpdateComplete(bool success);

  // Installs the validated external extension into cache.
  void PutValidatedExternalExtension(
      const std::string& app_id,
      const base::FilePath& crx_path,
      const std::string& version,
      const ExternalCache::PutExternalExtensionCallback& callback);

  // Whether the current platform is compliant with the given required
  // platform version.
  bool IsPlatformCompliant(const std::string& required_platform_version) const;

  // Whether the platform is compliant for the given app.
  bool IsPlatformCompliantWithApp(const extensions::Extension* app) const;

  // Notifies the KioskAppManager that a given app was auto-launched
  // automatically with no delay on startup. Certain privacy-sensitive
  // kiosk-mode behavior (such as network reporting) is only enabled for
  // kiosk apps that are immediately auto-launched on startup.
  void SetAppWasAutoLaunchedWithZeroDelay(const std::string& app_id);

  // Initialize |app_session_|.
  void InitSession(Profile* profile, const std::string& app_id);

  AppSession* app_session() { return app_session_.get(); }
  bool external_loader_created() const { return external_loader_created_; }
  bool secondary_app_external_loader_created() const {
    return secondary_app_external_loader_created_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<KioskAppManager>;
  friend std::default_delete<KioskAppManager>;
  friend class KioskAppManagerTest;
  friend class KioskTest;
  friend class KioskUpdateTest;

  enum AutoLoginState {
    AUTOLOGIN_NONE      = 0,
    AUTOLOGIN_REQUESTED = 1,
    AUTOLOGIN_APPROVED  = 2,
    AUTOLOGIN_REJECTED  = 3,
  };

  KioskAppManager();
  ~KioskAppManager() override;

  // Stop all data loading and remove its dependency on CrosSettings.
  void CleanUp();

  // Gets KioskAppData for the given app id.
  const KioskAppData* GetAppData(const std::string& app_id) const;
  KioskAppData* GetAppDataMutable(const std::string& app_id);

  // Updates app data |apps_| based on CrosSettings.
  void UpdateAppData();

  // Clear cached data and crx of the removed apps.
  void ClearRemovedApps(
      const std::map<std::string, std::unique_ptr<KioskAppData>>& old_apps);

  // Updates the prefs of |external_cache_| from |apps_|.
  void UpdateExternalCachePrefs();

  // KioskAppDataDelegate overrides:
  void GetKioskAppIconCacheDir(base::FilePath* cache_dir) override;
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;

  // ExternalCache::Delegate:
  void OnExtensionListsUpdated(const base::DictionaryValue* prefs) override;
  void OnExtensionLoadedInCache(const std::string& id) override;
  void OnExtensionDownloadFailed(
      const std::string& id,
      extensions::ExtensionDownloaderDelegate::Error error) override;

  // Callback for EnterpriseInstallAttributes::LockDevice() during
  // EnableConsumerModeKiosk() call.
  void OnLockDevice(
      const EnableKioskAutoLaunchCallback& callback,
      policy::EnterpriseInstallAttributes::LockResult result);

  // Callback for EnterpriseInstallAttributes::ReadImmutableAttributes() during
  // GetConsumerKioskModeStatus() call.
  void OnReadImmutableAttributes(
      const GetConsumerKioskAutoLaunchStatusCallback& callback);

  // Callback for reading handling checks of the owner public.
  void OnOwnerFileChecked(
      const GetConsumerKioskAutoLaunchStatusCallback& callback,
      bool* owner_present);

  // Reads/writes auto login state from/to local state.
  AutoLoginState GetAutoLoginState() const;
  void SetAutoLoginState(AutoLoginState state);

  void GetCrxCacheDir(base::FilePath* cache_dir);
  void GetCrxUnpackDir(base::FilePath* unpack_dir);

  // Returns the auto launch delay.
  base::TimeDelta GetAutoLaunchDelay() const;

  // True if machine ownership is already established.
  bool ownership_established_;
  std::vector<std::unique_ptr<KioskAppData>> apps_;
  std::string auto_launch_app_id_;
  std::string currently_auto_launched_with_zero_delay_app_;
  base::ObserverList<KioskAppManagerObserver, true> observers_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;

  std::unique_ptr<ExternalCache> external_cache_;

  std::unique_ptr<KioskExternalUpdater> usb_stick_updater_;

  // The extension external loader for deploying primary app.
  bool external_loader_created_;
  base::WeakPtr<KioskAppExternalLoader> external_loader_;

  // The extension external loader for deploying secondary apps.
  bool secondary_app_external_loader_created_;
  base::WeakPtr<KioskAppExternalLoader> secondary_app_external_loader_;

  std::unique_ptr<AppSession> app_session_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
