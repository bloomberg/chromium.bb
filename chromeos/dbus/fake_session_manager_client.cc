// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_session_manager_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {

FakeSessionManagerClient::FakeSessionManagerClient()
    : start_device_wipe_call_count_(0),
      notify_lock_screen_shown_call_count_(0),
      notify_lock_screen_dismissed_call_count_(0),
      arc_available_(false) {}

FakeSessionManagerClient::~FakeSessionManagerClient() {
}

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
}

void FakeSessionManagerClient::RestartJob(
    const std::vector<std::string>& argv) {}

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

void FakeSessionManagerClient::RequestLockScreen() {
}

void FakeSessionManagerClient::NotifyLockScreenShown() {
  notify_lock_screen_shown_call_count_++;
}

void FakeSessionManagerClient::NotifyLockScreenDismissed() {
  notify_lock_screen_dismissed_call_count_++;
}

void FakeSessionManagerClient::RetrieveActiveSessions(
      const ActiveSessionsCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, user_sessions_, true));
}

void FakeSessionManagerClient::RetrieveDevicePolicy(
    const RetrievePolicyCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, device_policy_));
}

void FakeSessionManagerClient::RetrievePolicyForUser(
    const cryptohome::Identification& cryptohome_id,
    const RetrievePolicyCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, user_policies_[cryptohome_id]));
}

std::string FakeSessionManagerClient::BlockingRetrievePolicyForUser(
    const cryptohome::Identification& cryptohome_id) {
  return user_policies_[cryptohome_id];
}

void FakeSessionManagerClient::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    const RetrievePolicyCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, device_local_account_policy_[account_id]));
}

void FakeSessionManagerClient::StoreDevicePolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_policy_ = policy_blob;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
  FOR_EACH_OBSERVER(Observer, observers_, PropertyChangeComplete(true));
}

void FakeSessionManagerClient::StorePolicyForUser(
    const cryptohome::Identification& cryptohome_id,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  user_policies_[cryptohome_id] = policy_blob;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

void FakeSessionManagerClient::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_local_account_policy_[account_id] = policy_blob;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

void FakeSessionManagerClient::SetFlagsForUser(
    const cryptohome::Identification& cryptohome_id,
    const std::vector<std::string>& flags) {}

void FakeSessionManagerClient::GetServerBackedStateKeys(
    const StateKeysCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, server_backed_state_keys_));
}

void FakeSessionManagerClient::CheckArcAvailability(
    const ArcCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, arc_available_));
}

void FakeSessionManagerClient::StartArcInstance(const std::string& socket_path,
                                                const ArcCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, arc_available_));
}

void FakeSessionManagerClient::StopArcInstance(const ArcCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, arc_available_));
}

void FakeSessionManagerClient::GetArcStartTime(
    const GetArcStartTimeCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, arc_available_, base::TimeTicks::Now()));
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
  FOR_EACH_OBSERVER(Observer, observers_, PropertyChangeComplete(success));
}

}  // namespace chromeos
