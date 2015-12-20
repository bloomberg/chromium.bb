// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/arc/arc_bridge_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/layout.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Declares shareable ARC app specific preferences, that keep information
// about app attributes (name, package, activity) and its state. This
// information is used to pre-create non-ready app items while ARC bridge
// service is not ready to provide information about available ARC apps.
class ArcAppListPrefs : public KeyedService,
                        public arc::ArcBridgeService::Observer,
                        public arc::ArcBridgeService::AppObserver {
 public:
  struct AppInfo {
    AppInfo(const std::string& name,
            const std::string& package,
            const std::string& activity,
            bool ready);

    std::string name;
    std::string package;
    std::string activity;
    bool ready;
  };

  class Observer {
   public:
    // Notifies an observer that new app is registered.
    virtual void OnAppRegistered(const std::string& app_id,
                                 const AppInfo& app_info) = 0;
    // Notifies an observer that app ready state has been changed.
    virtual void OnAppReadyChanged(const std::string& id, bool ready) = 0;
    // Notifies an observer that app icon has been installed or updated.
    virtual void OnAppIconUpdated(const std::string& id,
                                  ui::ScaleFactor scale_factor) = 0;
   protected:
    virtual ~Observer() {}
  };

  static ArcAppListPrefs* Create(const base::FilePath& base_path,
                                 PrefService* prefs);

  // Convenience function to get the ArcAppListPrefs for a BrowserContext.
  static ArcAppListPrefs* Get(content::BrowserContext* context);

  // Constructs unique id based on package and activity information. This id
  // is safe to use at file paths and as preference keys.
  static std::string GetAppId(const std::string& package,
                              const std::string& activity);

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~ArcAppListPrefs() override;

  // Returns a list of all app ids, including ready and non-ready apps.
  std::vector<std::string> GetAppIds() const;

  // Extracts attributes of an app based on its id. Returns NULL if the app is
  // not found.
  scoped_ptr<AppInfo> GetApp(const std::string& app_id) const;

  // Constructs path to app icon for specific scale factor.
  base::FilePath GetIconPath(const std::string& app_id,
                             ui::ScaleFactor scale_factor) const;

  // Requests to load an app icon for specific scale factor. If the app or Arc
  // bridge service is not ready, then defer this request until the app gets
  // available. Once new icon is installed notifies an observer
  // OnAppIconUpdated.
  void RequestIcon(const std::string& app_id, ui::ScaleFactor scale_factor);

  // Returns true if app is registered.
  bool IsRegistered(const std::string& app_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // arc::ArcBridgeService::Observer
  void OnStateChanged(arc::ArcBridgeService::State state) override;
  void OnAppListRefreshed(const std::vector<arc::AppInfo>& apps) override;
  void OnAppIcon(const std::string& package,
                 const std::string& activity,
                 arc::ScaleFactor scale_factor,
                 const std::vector<uint8_t>& icon_png_data) override;

 private:
  // See the Create methods.
  ArcAppListPrefs(const base::FilePath& base_path, PrefService* prefs);

  void OnAppReady(const arc::AppInfo& app);
  void DisableAllApps();

  // Installs an icon to file system in the special folder of the profile
  // directory.
  void InstallIcon(const std::string& app_id,
                   ui::ScaleFactor scale_factor,
                   const std::vector<uint8>& contentPng);
  void OnIconInstalled(const std::string& app_id,
                       ui::ScaleFactor scale_factor,
                       bool install_succeed);

  // Owned by the BrowserContext.
  PrefService* prefs_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
  // Keeps root folder where ARC app icons for different scale factor are
  // stored.
  base::FilePath base_path_;
  // Contains set of ARC apps that are currently ready.
  std::set<std::string> ready_apps_;
  // Keeps deferred icon load requests. Each app may contain several requests
  // for different scale factor. Scale factor is defined by specific bit
  // position.
  std::map<std::string, uint32> request_icon_deferred_;

  base::WeakPtrFactory<ArcAppListPrefs> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppListPrefs);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LIST_PREFS_H_
