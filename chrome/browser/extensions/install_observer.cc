// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_observer.h"

namespace extensions {

InstallObserver::ExtensionInstallParams::ExtensionInstallParams(
    std::string extension_id,
    std::string extension_name,
    gfx::ImageSkia installing_icon,
    bool is_app,
    bool is_platform_app)
        : extension_id(extension_id),
          extension_name(extension_name),
          installing_icon(installing_icon),
          is_app(is_app),
          is_platform_app(is_platform_app),
          is_ephemeral(false) {}

void InstallObserver::OnBeginExtensionInstall(
    const ExtensionInstallParams& params) {}
void InstallObserver::OnDownloadProgress(const std::string& extension_id,
                                         int percent_downloaded) {}
void InstallObserver::OnInstallFailure(const std::string& extension_id) {}
void InstallObserver::OnExtensionInstalled(const Extension* extension) {}
void InstallObserver::OnExtensionLoaded(const Extension* extension) {}
void InstallObserver::OnExtensionUnloaded(const Extension* extension) {}
void InstallObserver::OnExtensionUninstalled(const Extension* extension) {}
void InstallObserver::OnDisabledExtensionUpdated(const Extension* extension) {}
void InstallObserver::OnAppInstalledToAppList(const std::string& extension_id) {
}
void InstallObserver::OnAppsReordered() {}
void InstallObserver::OnShutdown() {}

}  // namespace extensions
