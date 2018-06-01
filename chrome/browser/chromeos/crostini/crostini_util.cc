// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppNamePrefix[] = "_crostini_";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniAppLaunchAppType {
  // An app which isn't in the CrostiniAppRegistry. This shouldn't happen.
  kUnknownApp = 0,

  // The main terminal app.
  kTerminal = 1,

  // An app for which there is something in the CrostiniAppRegistry.
  kRegisteredApp = 2,

  kCount
};

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type,
                                CrostiniAppLaunchAppType::kCount);
}

void OnLaunchFailed(const std::string& app_id) {
  // TODO(timloh): Consider displaying a notification of some sort.
}

void OnCrostiniRestarted(const std::string& app_id,
                         base::OnceClosure callback,
                         crostini::ConciergeClientResult result) {
  if (result != crostini::ConciergeClientResult::SUCCESS) {
    OnLaunchFailed(app_id);
    return;
  }
  std::move(callback).Run();
}

void OnContainerApplicationLaunched(const std::string& app_id,
                                    crostini::ConciergeClientResult result) {
  if (result != crostini::ConciergeClientResult::SUCCESS)
    OnLaunchFailed(app_id);
}

void LaunchTerminal(Profile* profile) {
  crostini::CrostiniManager::GetInstance()->LaunchContainerTerminal(
      profile, kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
}

void LaunchContainerApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration) {
  crostini::CrostiniManager::GetInstance()->LaunchContainerApplication(
      profile, registration.VmName(), registration.ContainerName(),
      registration.DesktopFileId(),
      base::BindOnce(OnContainerApplicationLaunched, app_id));
}

}  // namespace

bool IsCrostiniAllowed() {
  return virtual_machines::AreVirtualMachinesAllowedByVersionAndChannel() &&
         virtual_machines::AreVirtualMachinesAllowedByPolicy() &&
         base::FeatureList::IsEnabled(features::kCrostini);
}

bool IsCrostiniUIAllowedForProfile(Profile* profile) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }

  return IsCrostiniAllowed() &&
         base::FeatureList::IsEnabled(features::kExperimentalCrostiniUI);
}

bool IsCrostiniEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

void LaunchCrostiniApp(Profile* profile, const std::string& app_id) {
  auto* crostini_manager = crostini::CrostiniManager::GetInstance();
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    LOG(ERROR) << "LaunchCrostiniApp called with an unknown app_id: " << app_id;
    return;
  }

  // Store these as we move |registration| into LaunchContainerApplication().
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();

  base::OnceClosure launch_closure;
  if (app_id == kCrostiniTerminalId) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kTerminal);

    if (!crostini_manager->IsCrosTerminaInstalled() ||
        !IsCrostiniEnabled(profile)) {
      ShowCrostiniInstallerView(profile, CrostiniUISurface::kAppList);
      return;
    }

    launch_closure = base::BindOnce(&LaunchTerminal, profile);
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
    launch_closure = base::BindOnce(&LaunchContainerApplication, profile,
                                    app_id, std::move(*registration));
  }

  // Update the last launched time.
  registry_service->AppLaunched(app_id);

  crostini_manager->RestartCrostini(
      profile, vm_name, container_name,
      base::BindOnce(OnCrostiniRestarted, app_id, std::move(launch_closure)));
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string ContainerUserNameForProfile(Profile* profile) {
  // Get rid of the @domain.name in the profile user name (an email address).
  std::string container_username = profile->GetProfileUserName();
  if (container_username.find('@') != std::string::npos) {
    // gaia::CanonicalizeEmail CHECKs its argument contains'@'.
    container_username = gaia::CanonicalizeEmail(container_username);
    // |container_username| may have changed, so we have to find again.
    return container_username.substr(0, container_username.find('@'));
  }
  return container_username;
}

std::string AppNameFromCrostiniAppId(const std::string& id) {
  return kCrostiniAppNamePrefix + id;
}

base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name) {
  if (!base::StartsWith(app_name, kCrostiniAppNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    return base::nullopt;
  }
  return app_name.substr(strlen(kCrostiniAppNamePrefix));
}
