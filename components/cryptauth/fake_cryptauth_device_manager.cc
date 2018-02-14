// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/fake_cryptauth_device_manager.h"

#include <memory>

namespace cryptauth {

FakeCryptAuthDeviceManager::FakeCryptAuthDeviceManager() = default;

FakeCryptAuthDeviceManager::~FakeCryptAuthDeviceManager() = default;

void FakeCryptAuthDeviceManager::FinishActiveSync(
    SyncResult sync_result,
    DeviceChangeResult device_change_result,
    const base::Time& sync_finish_time) {
  DCHECK(is_sync_in_progress_);
  is_sync_in_progress_ = false;

  if (sync_result == CryptAuthDeviceManager::SyncResult::SUCCESS) {
    last_sync_time_ = sync_finish_time;
    is_recovering_from_failure_ = false;
  } else {
    is_recovering_from_failure_ = true;
  }

  NotifySyncFinished(sync_result, device_change_result);
}

void FakeCryptAuthDeviceManager::Start() {
  has_started_ = true;
}

void FakeCryptAuthDeviceManager::ForceSyncNow(
    InvocationReason invocation_reason) {
  is_sync_in_progress_ = true;
  NotifySyncStarted();
}

base::Time FakeCryptAuthDeviceManager::GetLastSyncTime() const {
  return last_sync_time_;
}

base::TimeDelta FakeCryptAuthDeviceManager::GetTimeToNextAttempt() const {
  return time_to_next_attempt_;
}

bool FakeCryptAuthDeviceManager::IsSyncInProgress() const {
  return is_sync_in_progress_;
}

bool FakeCryptAuthDeviceManager::IsRecoveringFromFailure() const {
  return is_recovering_from_failure_;
}

std::vector<ExternalDeviceInfo> FakeCryptAuthDeviceManager::GetSyncedDevices()
    const {
  return synced_devices_;
}

std::vector<ExternalDeviceInfo> FakeCryptAuthDeviceManager::GetUnlockKeys()
    const {
  return unlock_keys_;
}

std::vector<ExternalDeviceInfo> FakeCryptAuthDeviceManager::GetPixelUnlockKeys()
    const {
  return pixel_unlock_keys_;
}

std::vector<ExternalDeviceInfo> FakeCryptAuthDeviceManager::GetTetherHosts()
    const {
  return tether_hosts_;
}

std::vector<ExternalDeviceInfo>
FakeCryptAuthDeviceManager::GetPixelTetherHosts() const {
  return pixel_tether_hosts_;
}

}  // namespace cryptauth
