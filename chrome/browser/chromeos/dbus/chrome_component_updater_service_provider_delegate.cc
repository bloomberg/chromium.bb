// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_component_updater_service_provider_delegate.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_installer.h"

namespace chromeos {

namespace {

std::string ErrorToString(
    component_updater::CrOSComponentManager::Error error) {
  switch (error) {
    case component_updater::CrOSComponentManager::Error::NONE:
      return "NONE";
    case component_updater::CrOSComponentManager::Error::UNKNOWN_COMPONENT:
      return "UNKNOWN_COMPONENT";
    case component_updater::CrOSComponentManager::Error::INSTALL_FAILURE:
      return "INSTALL_FAILURE";
    case component_updater::CrOSComponentManager::Error::MOUNT_FAILURE:
      return "MOUNT_FAILURE";
    case component_updater::CrOSComponentManager::Error::
        COMPATIBILITY_CHECK_FAILED:
      return "COMPATIBILITY_CHECK_FAILED";
  }
  return "Unknown error code";
}

void OnLoadComponent(
    ChromeComponentUpdaterServiceProviderDelegate::LoadCallback load_callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& mount_path) {
  if (error != component_updater::CrOSComponentManager::Error::NONE) {
    LOG(ERROR) << "component updater Load API error: " << ErrorToString(error);
  }
  std::move(load_callback).Run(mount_path);
}

}  // namespace

ChromeComponentUpdaterServiceProviderDelegate::
    ChromeComponentUpdaterServiceProviderDelegate() {}

ChromeComponentUpdaterServiceProviderDelegate::
    ~ChromeComponentUpdaterServiceProviderDelegate() {}

void ChromeComponentUpdaterServiceProviderDelegate::LoadComponent(
    const std::string& name,
    bool mount,
    LoadCallback load_callback) {
  g_browser_process->platform_part()->cros_component_manager()->Load(
      name,
      mount ? component_updater::CrOSComponentManager::MountPolicy::kMount
            : component_updater::CrOSComponentManager::MountPolicy::kDontMount,
      base::BindOnce(OnLoadComponent, std::move(load_callback)));
}

bool ChromeComponentUpdaterServiceProviderDelegate::UnloadComponent(
    const std::string& name) {
  return g_browser_process->platform_part()->cros_component_manager()->Unload(
      name);
}

}  // namespace chromeos
