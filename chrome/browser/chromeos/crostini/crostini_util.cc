// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"
#include "chrome/common/chrome_features.h"

namespace {

void MaybeLaunchTerminal(Profile* profile,
                         crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    crostini::CrostiniManager::GetInstance()->LaunchContainerTerminal(
        profile, kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  }
}

void MaybeLaunchContainerAppplication(
    std::unique_ptr<crostini::CrostiniRegistryService::Registration>
        registration,
    crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    // TODO(timloh): Do something if launching failed, as otherwise the app
    // launcher remains open and there's no feedback.
    crostini::CrostiniManager::GetInstance()->LaunchContainerApplication(
        registration->vm_name, registration->container_name,
        registration->desktop_file_id, base::DoNothing());
  }
}

}  // namespace

bool IsCrostiniAllowed() {
  return virtual_machines::AreVirtualMachinesAllowedByVersionAndChannel() &&
         virtual_machines::AreVirtualMachinesAllowedByPolicy();
}

bool IsExperimentalCrostiniUIAvailable() {
  return IsCrostiniAllowed() &&
         base::FeatureList::IsEnabled(features::kExperimentalCrostiniUI);
}

bool IsCrostiniInstalled() {
  return false;
}

bool IsCrostiniRunning() {
  return false;
}

void LaunchCrostiniApp(Profile* profile, const std::string& app_id) {
  auto* crostini_manager = crostini::CrostiniManager::GetInstance();

  if (app_id == kCrostiniTerminalId) {
    if (!crostini_manager->IsCrosTerminaInstalled()) {
      CrostiniInstallerView::Show(profile);
    } else {
      crostini_manager->RestartCrostini(
          profile, kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
          base::BindOnce(&MaybeLaunchTerminal, profile));
    }
    return;
  }

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  std::unique_ptr<crostini::CrostiniRegistryService::Registration>
      registration = registry_service->GetRegistration(app_id);
  if (!registration) {
    LOG(ERROR) << "LaunchCrostiniApp called with an unknown app_id: " << app_id;
    return;
  }
  crostini_manager->RestartCrostini(
      profile, registration->vm_name, registration->container_name,
      base::BindOnce(&MaybeLaunchContainerAppplication,
                     std::move(registration)));
}
