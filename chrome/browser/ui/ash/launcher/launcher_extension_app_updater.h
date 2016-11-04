// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {
class ExtensionSet;
}  // namespace extensions

class LauncherExtensionAppUpdater
    : public LauncherAppUpdater,
      public extensions::ExtensionRegistryObserver,
      public ArcAppListPrefs::Observer {
 public:
  LauncherExtensionAppUpdater(Delegate* delegate,
                              content::BrowserContext* browser_context);
  ~LauncherExtensionAppUpdater() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // ArcAppListPrefs::Observer
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name) override;

 private:
  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  void UpdateHostedApps();
  void UpdateHostedApps(const extensions::ExtensionSet& extensions);
  void UpdateHostedApp(const std::string& app_id);
  void UpdateEquivalentHostedApp(const std::string& arc_package_name);

  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LauncherExtensionAppUpdater);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
