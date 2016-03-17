// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"
#include "extensions/browser/extension_registry_observer.h"

class LauncherExtensionAppUpdater
    : public LauncherAppUpdater,
      public extensions::ExtensionRegistryObserver {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherExtensionAppUpdater);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_EXTENSION_APP_UPDATER_H_
