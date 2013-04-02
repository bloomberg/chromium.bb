// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class RefCountedString;
}

namespace chromeos {

class KioskAppData;
class KioskAppManagerObserver;

// KioskAppManager manages cached app data.
class KioskAppManager : public content::NotificationObserver,
                        public KioskAppDataDelegate {
 public:
  // Struct to hold app info returned from GetApps() call.
  struct App {
    explicit App(const KioskAppData& data);
    App();
    ~App();

    std::string id;
    std::string name;
    gfx::ImageSkia icon;
    bool is_loading;
  };
  typedef std::vector<App> Apps;

  // Name of a dictionary that holds kiosk app info.
  // Sample layout:
  //   "kiosk": {
  //     "auto_launch": "app_id1",  // Exists if using local state pref
  //     "apps": {
  //       "app_id1" : {
  //         "name": "name of app1",
  //         "icon": "path to locally saved icon file"
  //       },
  //       "app_id2": {
  //         "name": "name of app2",
  //         "icon": "path to locally saved icon file"
  //       },
  //       ...
  //     }
  //   }
  static const char kKioskDictionaryName[];
  static const char kKeyApps[];

  // Sub directory under DIR_USER_DATA to store cached icon files.
  static const char kIconCacheDir[];

  // Gets the KioskAppManager instance, which is lazily created on first call..
  static KioskAppManager* Get();

  // Prepares for shutdown and calls CleanUp() if needed.
  static void Shutdown();

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns auto launcher app id or an empty string if there is none.
  std::string GetAutoLaunchApp() const;

  // Sets |app_id| as the app to auto launch at start up.
  void SetAutoLaunchApp(const std::string& app_id);

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

  KioskAppManager();
  virtual ~KioskAppManager();

  // Stop all data loading and remove its dependency on CrosSettings.
  void CleanUp();

  // Gets KioskAppData for the given app id.
  const KioskAppData* GetAppData(const std::string& app_id) const;

  // Update app data |apps_| based on |prefs_|.
  void UpdateAppData();

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // KioskAppDataDelegate overrides:
  virtual void GetKioskAppIconCacheDir(base::FilePath* cache_dir) OVERRIDE;
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE;
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE;

  ScopedVector<KioskAppData> apps_;
  ObserverList<KioskAppManagerObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
