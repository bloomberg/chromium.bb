// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_session_manager_client.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

using RetrievePolicyResponseType =
    chromeos::FakeSessionManagerClient::RetrievePolicyResponseType;

namespace chromeos {

namespace {

constexpr char kFakeContainerInstanceId[] = "0123456789ABCDEF";

// Store the owner key in a file on the disk, so that it can be loaded by
// DeviceSettingsService and used e.g. for validating policy signatures in the
// integration tests. This is done on behalf of the real session manager, that
// would be managing the owner key file on Chrome OS.
bool StoreOwnerKey(const std::string& public_key) {
  base::FilePath owner_key_path;
  if (!base::PathService::Get(FILE_OWNER_KEY, &owner_key_path)) {
    LOG(ERROR) << "Failed to obtain the path to the owner key file";
    return false;
  }
  if (!base::CreateDirectory(owner_key_path.DirName())) {
    LOG(ERROR) << "Failed to create the directory for the owner key file";
    return false;
  }
  if (base::WriteFile(owner_key_path, public_key.c_str(),
                      public_key.length()) !=
      base::checked_cast<int>(public_key.length())) {
    LOG(ERROR) << "Failed to store the owner key file";
    return false;
  }
  return true;
}

}  // namespace

FakeSessionManagerClient::FakeSessionManagerClient()
    : start_device_wipe_call_count_(0),
      request_lock_screen_call_count_(0),
      notify_lock_screen_shown_call_count_(0),
      notify_lock_screen_dismissed_call_count_(0),
      arc_available_(false),
      weak_ptr_factory_(this) {}

FakeSessionManagerClient::~FakeSessionManagerClient() = default;

void FakeSessionManagerClient::Init(dbus::Bus* bus) {
}

void FakeSessionManagerClient::SetStubDelegate(StubDelegate* delegate) {
}

void FakeSessionManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSessionManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeSessionManagerClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

bool FakeSessionManagerClient::IsScreenLocked() const {
  return false;
}

void FakeSessionManagerClient::EmitLoginPromptVisible() {
  for (auto& observer : observers_)
    observer.EmitLoginPromptVisibleCalled();
}

void FakeSessionManagerClient::RestartJob(int socket_fd,
                                          const std::vector<std::string>& argv,
                                          VoidDBusMethodCallback callback) {}

void FakeSessionManagerClient::StartSession(
    const cryptohome::Identification& cryptohome_id) {
  DCHECK_EQ(0UL, user_sessions_.count(cryptohome_id));
  std::string user_id_hash =
      CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
  user_sessions_[cryptohome_id] = user_id_hash;
}

void FakeSessionManagerClient::StopSession() {
}

void FakeSessionManagerClient::NotifySupervisedUserCreationStarted() {
}

void FakeSessionManagerClient::NotifySupervisedUserCreationFinished() {
}

void FakeSessionManagerClient::StartDeviceWipe() {
  start_device_wipe_call_count_++;
}

void FakeSessionManagerClient::StartTPMFirmwareUpdate(
    const std::string& update_mode) {}

void FakeSessionManagerClient::RequestLockScreen() {
  request_lock_screen_call_count_++;
}

void FakeSessionManagerClient::NotifyLockScreenShown() {
  notify_lock_screen_shown_call_count_++;
}

void FakeSessionManagerClient::NotifyLockScreenDismissed() {
  notify_lock_screen_dismissed_call_count_++;
}

void FakeSessionManagerClient::RetrieveActiveSessions(
    ActiveSessionsCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), user_sessions_));
}

void FakeSessionManagerClient::RetrieveDevicePolicy(
    RetrievePolicyCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), RetrievePolicyResponseType::SUCCESS,
                     device_policy_));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrieveDevicePolicy(
    std::string* policy_out) {
  *policy_out = device_policy_;
  return RetrievePolicyResponseType::SUCCESS;
}

void FakeSessionManagerClient::RetrievePolicyForUser(
    const cryptohome::Identification& cryptohome_id,
    RetrievePolicyCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), RetrievePolicyResponseType::SUCCESS,
                     user_policies_[cryptohome_id]));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrievePolicyForUser(
    const cryptohome::Identification& cryptohome_id,
    std::string* policy_out) {
  *policy_out = user_policies_[cryptohome_id];
  return RetrievePolicyResponseType::SUCCESS;
}

void FakeSessionManagerClient::RetrievePolicyForUserWithoutSession(
    const cryptohome::Identification& cryptohome_id,
    RetrievePolicyCallback callback) {
  auto iter = user_policies_without_session_.find(cryptohome_id);
  auto task =
      iter == user_policies_.end()
          ? base::BindOnce(std::move(callback),
                           RetrievePolicyResponseType::OTHER_ERROR,
                           std::string())
          : base::BindOnce(std::move(callback),
                           RetrievePolicyResponseType::SUCCESS, iter->second);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
}

void FakeSessionManagerClient::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    RetrievePolicyCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), RetrievePolicyResponseType::SUCCESS,
                     device_local_account_policy_[account_id]));
}

RetrievePolicyResponseType
FakeSessionManagerClient::BlockingRetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    std::string* policy_out) {
  *policy_out = device_local_account_policy_[account_id];
  return RetrievePolicyResponseType::SUCCESS;
}

void FakeSessionManagerClient::StoreDevicePolicy(
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  enterprise_management::PolicyFetchResponse policy;
  if (!policy.ParseFromString(policy_blob)) {
    LOG(ERROR) << "Unable to parse policy protobuf";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false /* success */));
    return;
  }

  if (!store_device_policy_success_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false /* success */));
    return;
  }

  bool owner_key_store_success = false;
  if (policy.has_new_public_key())
    owner_key_store_success = StoreOwnerKey(policy.new_public_key());
  device_policy_ = policy_blob;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
  if (policy.has_new_public_key()) {
    for (auto& observer : observers_)
      observer.OwnerKeySet(owner_key_store_success);
  }
  for (auto& observer : observers_)
    observer.PropertyChangeComplete(true /* success */);
}

void FakeSessionManagerClient::StorePolicyForUser(
    const cryptohome::Identification& cryptohome_id,
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  bool result = false;
  if (store_user_policy_success_) {
    user_policies_[cryptohome_id] = policy_blob;
    result = true;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeSessionManagerClient::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    VoidDBusMethodCallback callback) {
  device_local_account_policy_[account_id] = policy_blob;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeSessionManagerClient::SupportsRestartToApplyUserFlags() const {
  return false;
}

void FakeSessionManagerClient::SetFlagsForUser(
    const cryptohome::Identification& cryptohome_id,
    const std::vector<std::string>& flags) {}

void FakeSessionManagerClient::GetServerBackedStateKeys(
    StateKeysCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), server_backed_state_keys_));
}

void FakeSessionManagerClient::StartArcInstance(
    const login_manager::StartArcInstanceRequest& request,
    StartArcInstanceCallback callback) {
  last_start_arc_request_ = request;
  StartArcInstanceResult result;
  std::string container_instance_id;
  if (!arc_available_) {
    result = StartArcInstanceResult::UNKNOWN_ERROR;
  } else if (low_disk_) {
    result = StartArcInstanceResult::LOW_FREE_DISK_SPACE;
  } else {
    result = StartArcInstanceResult::SUCCESS;
    if (container_instance_id_.empty()) {
      // This is starting a new container.
      base::Base64Encode(kFakeContainerInstanceId, &container_instance_id_);
      // Note that empty |container_instance_id| should be returned if
      // this is upgrade case, so assign only when starting a new container.
      container_instance_id = container_instance_id_;
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result,
                                container_instance_id, base::ScopedFD()));
}

void FakeSessionManagerClient::StopArcInstance(
    VoidDBusMethodCallback callback) {
  if (!arc_available_ || container_instance_id_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false /* result */));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* result */));
  // Emulate ArcInstanceStopped signal propagation.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSessionManagerClient::NotifyArcInstanceStopped,
                     weak_ptr_factory_.GetWeakPtr(), true /* clean */,
                     std::move(container_instance_id_)));
  container_instance_id_.clear();
}

void FakeSessionManagerClient::SetArcCpuRestriction(
    login_manager::ContainerCpuRestrictionState restriction_state,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), arc_available_));
}

void FakeSessionManagerClient::EmitArcBooted(
    const cryptohome::Identification& cryptohome_id,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), arc_available_));
}

void FakeSessionManagerClient::GetArcStartTime(
    DBusMethodCallback<base::TimeTicks> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     arc_available_ ? base::make_optional(arc_start_time_)
                                    : base::nullopt));
}

void FakeSessionManagerClient::RemoveArcData(
    const cryptohome::Identification& cryptohome_id,
    VoidDBusMethodCallback callback) {
  if (!callback.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), arc_available_));
  }
}

void FakeSessionManagerClient::NotifyArcInstanceStopped(
    bool clean,
    const std::string& container_instance_id) {
  for (auto& observer : observers_)
    observer.ArcInstanceStopped(clean, container_instance_id);
}

const std::string& FakeSessionManagerClient::device_policy() const {
  return device_policy_;
}

void FakeSessionManagerClient::set_device_policy(
    const std::string& policy_blob) {
  device_policy_ = policy_blob;
}

const std::string& FakeSessionManagerClient::user_policy(
    const cryptohome::Identification& cryptohome_id) const {
  std::map<cryptohome::Identification, std::string>::const_iterator it =
      user_policies_.find(cryptohome_id);
  return it == user_policies_.end() ? base::EmptyString() : it->second;
}

void FakeSessionManagerClient::set_user_policy(
    const cryptohome::Identification& cryptohome_id,
    const std::string& policy_blob) {
  user_policies_[cryptohome_id] = policy_blob;
}

void FakeSessionManagerClient::set_user_policy_without_session(
    const cryptohome::Identification& cryptohome_id,
    const std::string& policy_blob) {
  user_policies_without_session_[cryptohome_id] = policy_blob;
}

const std::string& FakeSessionManagerClient::device_local_account_policy(
    const std::string& account_id) const {
  std::map<std::string, std::string>::const_iterator entry =
      device_local_account_policy_.find(account_id);
  return entry != device_local_account_policy_.end() ? entry->second
                                                     : base::EmptyString();
}

void FakeSessionManagerClient::set_device_local_account_policy(
    const std::string& account_id,
    const std::string& policy_blob) {
  device_local_account_policy_[account_id] = policy_blob;
}

void FakeSessionManagerClient::OnPropertyChangeComplete(bool success) {
  for (auto& observer : observers_)
    observer.PropertyChangeComplete(success);
}

}  // namespace chromeos
