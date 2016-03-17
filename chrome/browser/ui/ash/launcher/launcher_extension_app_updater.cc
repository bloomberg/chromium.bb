// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"

#include "extensions/browser/extension_registry.h"

LauncherExtensionAppUpdater::LauncherExtensionAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context)
    : LauncherAppUpdater(delegate, browser_context) {
  extensions::ExtensionRegistry::Get(browser_context)->AddObserver(this);
}

LauncherExtensionAppUpdater::~LauncherExtensionAppUpdater() {
  extensions::ExtensionRegistry::Get(browser_context())->RemoveObserver(this);
}

void LauncherExtensionAppUpdater::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  delegate()->OnAppInstalled(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (reason == extensions::UnloadedExtensionInfo::REASON_UNINSTALL)
    delegate()->OnAppUninstalled(browser_context, extension->id());
  else
    delegate()->OnAppUpdated(browser_context, extension->id());
}
