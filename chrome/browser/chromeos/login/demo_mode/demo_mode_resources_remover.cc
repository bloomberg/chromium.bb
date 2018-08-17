// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "third_party/re2/src/re2/re2.h"

namespace chromeos {

namespace {

DemoModeResourcesRemover* g_instance = nullptr;

// Name of the pref in local state that indicates whether demo mode resources
// have been removed from the device.
constexpr char kDemoModeResourcesRemoved[] = "demo_mode_resources_removed";

// Regex matching legacy demo retail mode domains.
constexpr char kLegacyDemoRetailModeDomainRegex[] =
    "[[:alpha:]]{2}-retailmode.com";

// An extra legacy demo retail mode domain that does not match
// |kLegacyDemoRetailModeDomainRegex|.
constexpr char kExtraLegacyDemoRetailModeDomain[] = "us2-retailmode.com";

// Deletes directory at |path| from the device.
DemoModeResourcesRemover::RemovalResult RemoveDirectory(
    const base::FilePath& path) {
  if (!base::DirectoryExists(path) || base::IsDirectoryEmpty(path))
    return DemoModeResourcesRemover::RemovalResult::kNotFound;

  if (!base::DeleteFile(path, true /*recursive*/))
    return DemoModeResourcesRemover::RemovalResult::kFailed;

  return DemoModeResourcesRemover::RemovalResult::kSuccess;
}

// Tests whether the session with user |user| is part of legacy demo mode -
// a public session in a legacy demo retail mode domain.
// Note that DemoSession::IsDeviceInDemoMode will return false for these
// sessions.
bool IsLegacyDemoRetailModeSession(const user_manager::User* user) {
  if (user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return false;

  const std::string enrollment_domain =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetEnterpriseEnrollmentDomain();
  return DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      enrollment_domain);
}

}  // namespace

// static
void DemoModeResourcesRemover::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDemoModeResourcesRemoved, false);
}

// static
std::unique_ptr<DemoModeResourcesRemover>
DemoModeResourcesRemover::CreateIfNeeded(PrefService* local_state) {
  if (DemoSession::IsDeviceInDemoMode() ||
      local_state->GetBoolean(kDemoModeResourcesRemoved)) {
    return nullptr;
  }

  return base::WrapUnique(new DemoModeResourcesRemover(local_state));
}

// static
DemoModeResourcesRemover* DemoModeResourcesRemover::Get() {
  return g_instance;
}

// static
bool DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
    const std::string& domain) {
  return RE2::FullMatch(domain, kLegacyDemoRetailModeDomainRegex) ||
         domain == kExtraLegacyDemoRetailModeDomain;
}

DemoModeResourcesRemover::~DemoModeResourcesRemover() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;

  ChromeUserManager::Get()->RemoveSessionStateObserver(this);
}

void DemoModeResourcesRemover::LowDiskSpace(uint64_t free_disk_space) {
  AttemptRemoval(RemovalReason::kLowDiskSpace, RemovalCallback());
}

void DemoModeResourcesRemover::ActiveUserChanged(
    const user_manager::User* user) {
  // Ignore user activity in guest sessions.
  if (user->GetType() == user_manager::USER_TYPE_GUEST)
    return;

  // Do not remove resources if the device is in a legacy derelict demo session,
  // which is implemented as kiosk - note that this is different than sessions
  // detected by IsLegacyDemoRetailModeSession().
  if (DemoAppLauncher::IsDemoAppSession(user->GetAccountId()))
    return;

  // Attempt resources removal if the device is managed, and not in a retail
  // mode domain.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() &&
      !IsLegacyDemoRetailModeSession(user)) {
    AttemptRemoval(RemovalReason::kEnterpriseEnrolled, RemovalCallback());
  }
}

void DemoModeResourcesRemover::AttemptRemoval(RemovalReason reason,
                                              RemovalCallback callback) {
  if (local_state_->GetBoolean(kDemoModeResourcesRemoved)) {
    if (callback)
      std::move(callback).Run(RemovalResult::kAlreadyRemoved);
    return;
  }

  if (DemoSession::IsDeviceInDemoMode()) {
    if (callback)
      std::move(callback).Run(RemovalResult::kNotAllowed);
    return;
  }

  if (callback)
    removal_callbacks_.push_back(std::move(callback));

  if (removal_in_progress_)
    return;
  removal_in_progress_ = true;

  // Report this metric only once per resources directory removal task.
  // Concurrent removal requests should not be reported multiple times.
  UMA_HISTOGRAM_ENUMERATION("DemoMode.ResourcesRemoval.Reason", reason);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemoveDirectory,
                     DemoSession::GetPreInstalledDemoResourcesPath()),
      base::BindOnce(&DemoModeResourcesRemover::OnRemovalDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

DemoModeResourcesRemover::DemoModeResourcesRemover(PrefService* local_state)
    : local_state_(local_state),
      cryptohome_observer_(this),
      weak_ptr_factory_(this) {
  CHECK(!g_instance);
  g_instance = this;

  cryptohome_observer_.Add(DBusThreadManager::Get()->GetCryptohomeClient());
  ChromeUserManager::Get()->AddSessionStateObserver(this);
}

void DemoModeResourcesRemover::OnRemovalDone(RemovalResult result) {
  DCHECK(removal_in_progress_);
  removal_in_progress_ = false;

  if (result == RemovalResult::kNotFound || result == RemovalResult::kSuccess) {
    local_state_->SetBoolean(kDemoModeResourcesRemoved, true);
    cryptohome_observer_.RemoveAll();
    ChromeUserManager::Get()->RemoveSessionStateObserver(this);
  }

  UMA_HISTOGRAM_ENUMERATION("DemoMode.ResourcesRemoval.Result", result);

  std::vector<RemovalCallback> callbacks;
  callbacks.swap(removal_callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(result);
}

}  // namespace chromeos
