// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/layout.h"

class PrefService;
class Profile;

namespace arc {
class ArcPackageSyncableService;
}  // namespace arc

namespace chromeos {
class ArcKioskAppService;
}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Declares shareable ARC app specific preferences, that keep information
// about app attributes (name, package_name, activity) and its state. This
// information is used to pre-create non-ready app items while ARC bridge
// service is not ready to provide information about available ARC apps.
// NOTE: ArcAppListPrefs is only created for the primary user.
class ArcAppListPrefs
    : public KeyedService,
      public arc::mojom::AppHost,
      public arc::InstanceHolder<arc::mojom::AppInstance>::Observer,
      public arc::ArcSessionManager::Observer,
      public ArcDefaultAppList::Delegate {
 public:
  struct AppInfo {
    AppInfo(const std::string& name,
            const std::string& package_name,
            const std::string& activity,
            const std::string& intent_uri,
            const std::string& icon_resource_id,
            const base::Time& last_launch_time,
            const base::Time& install_time,
            bool sticky,
            bool notifications_enabled,
            bool ready,
            bool showInLauncher,
            bool shortcut,
            bool launchable,
            arc::mojom::OrientationLock orientation_lock);
    ~AppInfo();

    std::string name;
    std::string package_name;
    std::string activity;
    std::string intent_uri;
    std::string icon_resource_id;
    base::Time last_launch_time;
    base::Time install_time;
    bool sticky;
    bool notifications_enabled;
    bool ready;
    bool showInLauncher;
    bool shortcut;
    bool launchable;
    arc::mojom::OrientationLock orientation_lock;
  };

  struct PackageInfo {
    PackageInfo(const std::string& package_name,
                int32_t package_version,
                int64_t last_backup_android_id,
                int64_t last_backup_time,
                bool should_sync,
                bool system);

    std::string package_name;
    int32_t package_version;
    int64_t last_backup_android_id;
    int64_t last_backup_time;
    bool should_sync;
    bool system;
  };

  class Observer {
   public:
    // Notifies an observer that new app is registered.
    virtual void OnAppRegistered(const std::string& app_id,
                                 const AppInfo& app_info) {}
    // Notifies an observer that app ready state has been changed.
    virtual void OnAppReadyChanged(const std::string& id, bool ready) {}
    // Notifies an observer that app was removed.
    virtual void OnAppRemoved(const std::string& id) {}
    // Notifies an observer that app icon has been installed or updated.
    virtual void OnAppIconUpdated(const std::string& id,
                                  ui::ScaleFactor scale_factor) {}
    // Notifies an observer that the name of an app has changed.
    virtual void OnAppNameUpdated(const std::string& id,
                                  const std::string& name) {}
    // Notifies that task has been created and provides information about
    // initial activity.
    virtual void OnTaskCreated(int32_t task_id,
                               const std::string& package_name,
                               const std::string& activity,
                               const std::string& intent) {}
    // Notifies that task has been destroyed.
    virtual void OnTaskDestroyed(int32_t task_id) {}
    // Notifies that task has been activated and moved to the front.
    virtual void OnTaskSetActive(int32_t task_id) {}

    virtual void OnNotificationsEnabledChanged(
        const std::string& package_name, bool enabled) {}
    // Notifies that package has been installed.
    virtual void OnPackageInstalled(
        const arc::mojom::ArcPackageInfo& package_info) {}
    // Notifies that package has been modified.
    virtual void OnPackageModified(
        const arc::mojom::ArcPackageInfo& package_info) {}
    // Notifies that package has been uninstalled.
    virtual void OnPackageRemoved(const std::string& package_name) {}
    // Notifies sync date type controller the model is ready to start.
    virtual void OnPackageListInitialRefreshed() {}

    virtual void OnTaskOrientationLockRequested(
        int32_t task_id,
        const arc::mojom::OrientationLock orientation_lock) {}

   protected:
    virtual ~Observer() {}
  };

  static ArcAppListPrefs* Create(
      Profile* profile,
      arc::InstanceHolder<arc::mojom::AppInstance>* app_instance_holder);

  // Convenience function to get the ArcAppListPrefs for a BrowserContext. It
  // will only return non-null pointer for the primary user.
  static ArcAppListPrefs* Get(content::BrowserContext* context);

  // Constructs unique id based on package name and activity information. This
  // id is safe to use at file paths and as preference keys.
  static std::string GetAppId(const std::string& package_name,
                              const std::string& activity);

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~ArcAppListPrefs() override;

  // Returns a list of all app ids, including ready and non-ready apps.
  std::vector<std::string> GetAppIds() const;

  // Extracts attributes of an app based on its id. Returns NULL if the app is
  // not found.
  std::unique_ptr<AppInfo> GetApp(const std::string& app_id) const;

  // Get current installed package names.
  std::vector<std::string> GetPackagesFromPrefs() const;

  // Extracts attributes of a package based on its package name. Returns
  // nullptr if the package is not found.
  std::unique_ptr<PackageInfo> GetPackage(
      const std::string& package_name) const;

  // Constructs path to app local data.
  base::FilePath GetAppPath(const std::string& app_id) const;

  // Constructs path to app icon for specific scale factor.
  base::FilePath GetIconPath(const std::string& app_id,
                             ui::ScaleFactor scale_factor) const;
  // Constructs path to default app icon for specific scale factor. This path
  // is used to resolve icon if no icon is available at |GetIconPath|.
  base::FilePath MaybeGetIconPathForDefaultApp(
      const std::string& app_id,
      ui::ScaleFactor scale_factor) const;

  // Sets last launched time for the requested app.
  void SetLastLaunchTime(const std::string& app_id, const base::Time& time);

  // Requests to load an app icon for specific scale factor. If the app or Arc
  // bridge service is not ready, then defer this request until the app gets
  // available. Once new icon is installed notifies an observer
  // OnAppIconUpdated.
  void RequestIcon(const std::string& app_id, ui::ScaleFactor scale_factor);

  // Sets notification enabled flag for given value. If the app or Arc bridge
  // service is not ready, then defer this request until the app gets
  // available. Once new value is set notifies an observer
  // OnNotificationsEnabledChanged.
  void SetNotificationsEnabled(const std::string& app_id, bool enabled);

  // Returns true if app is registered.
  bool IsRegistered(const std::string& app_id) const;
  // Returns true if app is a default app.
  bool IsDefault(const std::string& app_id) const;
  // Returns true if app is an OEM app.
  bool IsOem(const std::string& app_id) const;
  // Returns true if app is a shortcut
  bool IsShortcut(const std::string& app_id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  // arc::ArcSessionManager::Observer:
  void OnOptInEnabled(bool enabled) override;

  // ArcDefaultAppList::Delegate:
  void OnDefaultAppsReady() override;

  // Removes app with the given app_id.
  void RemoveApp(const std::string& app_id);

  arc::InstanceHolder<arc::mojom::AppInstance>* app_instance_holder() {
    return app_instance_holder_;
  }

  bool package_list_initial_refreshed() const {
    return package_list_initial_refreshed_;
  }

  std::unordered_set<std::string> GetAppsForPackage(
      const std::string& package_name) const;

  void SetDefaltAppsReadyCallback(base::Closure callback);

 private:
  friend class ChromeLauncherControllerImplTest;

  // See the Create methods.
  ArcAppListPrefs(
      Profile* profile,
      arc::InstanceHolder<arc::mojom::AppInstance>* app_instance_holder);

  // arc::InstanceHolder<arc::mojom::AppInstance>::Observer:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // arc::mojom::AppHost:
  void OnAppListRefreshed(std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) override;
  void OnPackageAppListRefreshed(
      const std::string& package_name,
      std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnInstallShortcut(arc::mojom::ShortcutInfoPtr app) override;
  void OnPackageRemoved(const std::string& package_name) override;
  void OnAppIcon(const std::string& package_name,
                 const std::string& activity,
                 arc::mojom::ScaleFactor scale_factor,
                 const std::vector<uint8_t>& icon_png_data) override;
  void OnIcon(const std::string& app_id,
              arc::mojom::ScaleFactor scale_factor,
              const std::vector<uint8_t>& icon_png_data);
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const base::Optional<std::string>& name,
                     const base::Optional<std::string>& intent) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;
  void OnNotificationsEnabledChanged(const std::string& package_name,
                                     bool enabled) override;
  void OnPackageAdded(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageModified(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageListRefreshed(
      std::vector<arc::mojom::ArcPackageInfoPtr> packages) override;
  void OnTaskOrientationLockRequested(
      int32_t task_id,
      const arc::mojom::OrientationLock orientation_lock) override;

  void StartPrefs();

  void UpdateDefaultAppsHiddenState();
  void RegisterDefaultApps();

  // Returns list of packages from prefs. If |installed| is set to true then
  // returns currently installed packages. If not, returns list of packages that
  // where uninstalled. Note, we store uninstall packages only for packages of
  // default apps.
  std::vector<std::string> GetPackagesFromPrefs(bool installed) const;

  void AddApp(const arc::mojom::AppInfo& app_info);
  void AddAppAndShortcut(bool app_ready,
                         const std::string& name,
                         const std::string& package_name,
                         const std::string& activity,
                         const std::string& intent_uri,
                         const std::string& icon_resource_id,
                         const bool sticky,
                         const bool notifications_enabled,
                         const bool shortcut,
                         const bool launchable,
                         arc::mojom::OrientationLock orientation_lock);
  // Adds or updates local pref for given package.
  void AddOrUpdatePackagePrefs(PrefService* prefs,
                               const arc::mojom::ArcPackageInfo& package);
  // Removes given package from local pref.
  void RemovePackageFromPrefs(PrefService* prefs,
                              const std::string& package_name);

  void DisableAllApps();
  void RemoveAllApps();
  std::vector<std::string> GetAppIdsNoArcEnabledCheck() const;
  // Enumerates apps from preferences and notifies listeners about available
  // apps while Arc is not started yet. All apps in this case have disabled
  // state.
  void NotifyRegisteredApps();
  base::Time GetInstallTime(const std::string& app_id) const;

  // Installs an icon to file system in the special folder of the profile
  // directory.
  void InstallIcon(const std::string& app_id,
                   ui::ScaleFactor scale_factor,
                   const std::vector<uint8_t>& contentPng);
  void OnIconInstalled(const std::string& app_id,
                       ui::ScaleFactor scale_factor,
                       bool install_succeed);

  // This checks if app is not registered yet and in this case creates
  // non-launchable app entry.
  void MaybeAddNonLaunchableApp(const base::Optional<std::string>& name,
                                const std::string& package_name,
                                const std::string& activity);

  // Reveals first app from provided package in app launcher if package is newly
  // installed by user. If all apps in package are hidden then app list is not
  // shown.
  void MaybeShowPackageInAppLauncher(
      const arc::mojom::ArcPackageInfo& package_info);

  Profile* const profile_;

  // Owned by the BrowserContext.
  PrefService* const prefs_;

  arc::InstanceHolder<arc::mojom::AppInstance>* const app_instance_holder_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
  // Keeps root folder where ARC app icons for different scale factor are
  // stored.
  base::FilePath base_path_;
  // Contains set of ARC apps that are currently ready.
  std::unordered_set<std::string> ready_apps_;
  // Contains set of ARC apps that are currently tracked.
  std::unordered_set<std::string> tracked_apps_;
  // Keeps deferred icon load requests. Each app may contain several requests
  // for different scale factor. Scale factor is defined by specific bit
  // position.
  std::map<std::string, uint32_t> request_icon_deferred_;
  // True if this preference has been initialized once.
  bool is_initialized_ = false;
  // True if apps were restored.
  bool apps_restored_ = false;
  // True is Arc package list has been refreshed once.
  bool package_list_initial_refreshed_ = false;

  arc::ArcPackageSyncableService* sync_service_ = nullptr;
  // Track ARC kiosk app and auto-launches it if needed.
  chromeos::ArcKioskAppService* kiosk_app_service_ = nullptr;

  mojo::Binding<arc::mojom::AppHost> binding_;

  bool default_apps_ready_ = false;
  ArcDefaultAppList default_apps_;
  base::Closure default_apps_ready_callback_;

  base::WeakPtrFactory<ArcAppListPrefs> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppListPrefs);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
