// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

DemoModeResourcesRemover* g_instance = nullptr;

// Name of the pref in local state that indicates whether demo mode resources
// have been removed from the device.
constexpr char kDemoModeResourcesRemoved[] = "demo_mode_resources_removed";

// Deletes directory at |path| from the device.
DemoModeResourcesRemover::RemovalResult RemoveDirectory(
    const base::FilePath& path) {
  if (!base::DirectoryExists(path) || base::IsDirectoryEmpty(path))
    return DemoModeResourcesRemover::RemovalResult::kNotFound;

  if (!base::DeleteFile(path, true /*recursive*/))
    return DemoModeResourcesRemover::RemovalResult::kFailed;

  return DemoModeResourcesRemover::RemovalResult::kSuccess;
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

DemoModeResourcesRemover::~DemoModeResourcesRemover() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void DemoModeResourcesRemover::LowDiskSpace(uint64_t free_disk_space) {
  AttemptRemoval(RemovalReason::kLowDiskSpace, RemovalCallback());
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
}

void DemoModeResourcesRemover::OnRemovalDone(RemovalResult result) {
  DCHECK(removal_in_progress_);
  removal_in_progress_ = false;

  if (result == RemovalResult::kNotFound || result == RemovalResult::kSuccess) {
    local_state_->SetBoolean(kDemoModeResourcesRemoved, true);
    cryptohome_observer_.RemoveAll();
  }

  UMA_HISTOGRAM_ENUMERATION("DemoMode.ResourcesRemoval.Result", result);

  std::vector<RemovalCallback> callbacks;
  callbacks.swap(removal_callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(result);
}

}  // namespace chromeos
