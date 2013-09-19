// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class RefCountedString;
}

namespace chromeos {

class KioskAppData;
class KioskAppManagerObserver;

// KioskAppManager manages cached app data.
class KioskAppManager : public KioskAppDataDelegate {
 public:
  enum ConsumerKioskModeStatus {
    // Consumer kiosk mode can be enabled on this machine.
    CONSUMER_KIOSK_MODE_CONFIGURABLE,
    // Consumer kiosk is enabled on this machine.
    CONSUMER_KIOSK_MODE_ENABLED,
    // Consumer kiosk mode is disabled ans cannot any longer be enabled on
    // this machine.
    CONSUMER_KIOSK_MODE_DISABLED,
  };

  typedef base::Callback<void(bool success)> EnableKioskModeCallback;
  typedef base::Callback<void(ConsumerKioskModeStatus status)>
      GetConsumerKioskModeStatusCallback;

  // Struct to hold app info returned from GetApps() call.
  struct App {
    explicit App(const KioskAppData& data);
    App();
    ~App();

    std::string app_id;
    std::string user_id;
    std::string name;
    gfx::ImageSkia icon;
    bool is_loading;
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

  // Gets the KioskAppManager instance, which is lazily created on first call..
  static KioskAppManager* Get();

  // Prepares for shutdown and calls CleanUp() if needed.
  static void Shutdown();

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initiates reading of consumer kiosk mode status.
  void GetConsumerKioskModeStatus(
      const GetConsumerKioskModeStatusCallback& callback);

  // Enables consumer kiosk mode feature. Upon completion, |callback| will be
  // invoked with outcome of this operation.
  void EnableConsumerModeKiosk(const EnableKioskModeCallback& callback);

  // Returns auto launcher app id or an empty string if there is none.
  std::string GetAutoLaunchApp() const;

  // Sets |app_id| as the app to auto launch at start up.
  void SetAutoLaunchApp(const std::string& app_id);

  // Returns true if there is a pending auto-launch request.
  bool IsAutoLaunchRequested() const;

  // Returns true if owner/policy enabled auto launch.
  bool IsAutoLaunchEnabled() const;

  // Enable auto launch setter.
  void SetEnableAutoLaunch(bool value);

  // Adds/removes a kiosk app by id. When removed, all locally cached data
  // will be removed as well.
  void AddApp(const std::string& app_id);
  void RemoveApp(const std::string& app_id);

  // Gets info of all apps.
  void GetApps(Apps* apps) const;

  // Gets app data for the given app id. Returns true if |app_id| is known and
  // |app| is populated. Otherwise, return false.
  bool GetApp(const std::string& app_id, App* app) const;

  // Gets the raw icon data for the given app id. Returns NULL if |app_id|
  // is unknown.
  const base::RefCountedString* GetAppRawIcon(const std::string& app_id) const;

  // Gets whether the bailout shortcut is disabled.
  bool GetDisableBailoutShortcut() const;

  void AddObserver(KioskAppManagerObserver* observer);
  void RemoveObserver(KioskAppManagerObserver* observer);

 private:
  friend struct base::DefaultLazyInstanceTraits<KioskAppManager>;
  friend struct base::DefaultDeleter<KioskAppManager>;
  friend class KioskAppManagerTest;
  friend class KioskTest;

  enum AutoLoginState {
    AUTOLOGIN_NONE      = 0,
    AUTOLOGIN_REQUESTED = 1,
    AUTOLOGIN_APPROVED  = 2,
    AUTOLOGIN_REJECTED  = 3,
  };

  KioskAppManager();
  virtual ~KioskAppManager();

  // Stop all data loading and remove its dependency on CrosSettings.
  void CleanUp();

  // Gets KioskAppData for the given app id.
  const KioskAppData* GetAppData(const std::string& app_id) const;

  // Update app data |apps_| based on CrosSettings.
  void UpdateAppData();

  // KioskAppDataDelegate overrides:
  virtual void GetKioskAppIconCacheDir(base::FilePath* cache_dir) OVERRIDE;
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE;
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE;

  // Callback for EnterpriseInstallAttributes::LockDevice() during
  // EnableConsumerModeKiosk() call.
  void OnLockDevice(
      const EnableKioskModeCallback& callback,
      policy::EnterpriseInstallAttributes::LockResult result);

  // Callback for EnterpriseInstallAttributes::ReadImmutableAttributes() during
  // GetConsumerKioskModeStatus() call.
  void OnReadImmutableAttributes(
      const GetConsumerKioskModeStatusCallback& callback);

  // Callback for reading handling checks of the owner public.
  void OnOwnerFileChecked(
      const GetConsumerKioskModeStatusCallback& callback,
      bool *owner_present);

  // Reads/writes auto login state from/to local state.
  AutoLoginState GetAutoLoginState() const;
  void SetAutoLoginState(AutoLoginState state);

  // True if machine ownership is already established.
  bool ownership_established_;
  ScopedVector<KioskAppData> apps_;
  std::string auto_launch_app_id_;
  ObserverList<KioskAppManagerObserver, true> observers_;

  scoped_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
