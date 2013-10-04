// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_
#define CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/install_observer.h"

class Profile;

namespace app_list {

class RecommendedAppsObserver;

// A class that maintains a list of recommended apps by watching changes
// to app state.
class RecommendedApps : public extensions::InstallObserver {
 public:
  typedef std::vector<scoped_refptr<const extensions::Extension> > Apps;

  explicit RecommendedApps(Profile* profile);
  virtual ~RecommendedApps();

  void AddObserver(RecommendedAppsObserver* observer);
  void RemoveObserver(RecommendedAppsObserver* observer);

  const Apps& apps() const { return apps_; }

 private:
  void Update();

  // extensions::InstallObserver overrides:
  virtual void OnBeginExtensionInstall(const std::string& extension_id,
                                       const std::string& extension_name,
                                       const gfx::ImageSkia& installing_icon,
                                       bool is_app,
                                       bool is_platform_app) OVERRIDE;
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;
  virtual void OnInstallFailure(const std::string& extension_id) OVERRIDE;
  virtual void OnExtensionInstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionLoaded(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUninstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnAppsReordered() OVERRIDE;
  virtual void OnAppInstalledToAppList(
      const std::string& extension_id) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  Apps apps_;
  ObserverList<RecommendedAppsObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(RecommendedApps);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_RECOMMENDED_APPS_H_
