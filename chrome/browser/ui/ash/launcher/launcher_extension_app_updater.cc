// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"

#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "extensions/browser/extension_registry.h"

LauncherExtensionAppUpdater::LauncherExtensionAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context)
    : LauncherAppUpdater(delegate, browser_context) {
  StartObservingExtensionRegistry();

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context);
  if (prefs)
    prefs->AddObserver(this);
}

LauncherExtensionAppUpdater::~LauncherExtensionAppUpdater() {
  StopObservingExtensionRegistry();

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  if (prefs)
    prefs->RemoveObserver(this);
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
    delegate()->OnAppUninstalledPrepared(browser_context, extension->id());
  else
    delegate()->OnAppUpdated(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  delegate()->OnAppUninstalled(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

void LauncherExtensionAppUpdater::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  UpdateEquivalentHostedApp(package_info.package_name);
}

void LauncherExtensionAppUpdater::OnPackageRemoved(
    const std::string& package_name) {
  UpdateEquivalentHostedApp(package_name);
}

void LauncherExtensionAppUpdater::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ = extensions::ExtensionRegistry::Get(browser_context());
  extension_registry_->AddObserver(this);
}

void LauncherExtensionAppUpdater::StopObservingExtensionRegistry() {
  if (!extension_registry_)
    return;
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void LauncherExtensionAppUpdater::UpdateHostedApps() {
  if (!extension_registry_)
    return;

  UpdateHostedApps(extension_registry_->enabled_extensions());
  UpdateHostedApps(extension_registry_->disabled_extensions());
  UpdateHostedApps(extension_registry_->terminated_extensions());
}

void LauncherExtensionAppUpdater::UpdateHostedApps(
    const extensions::ExtensionSet& extensions) {
  content::BrowserContext* context = browser_context();
  extensions::ExtensionSet::const_iterator it;
  for (it = extensions.begin(); it != extensions.end(); ++it) {
    if ((*it)->is_hosted_app())
      delegate()->OnAppUpdated(context, (*it)->id());
  }
}

void LauncherExtensionAppUpdater::UpdateHostedApp(const std::string& app_id) {
  delegate()->OnAppUpdated(browser_context(), app_id);
}

void LauncherExtensionAppUpdater::UpdateEquivalentHostedApp(
    const std::string& arc_package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(browser_context(),
                                                         arc_package_name);
  for (const auto& iter : extension_ids)
    UpdateHostedApp(iter);
}
