// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_session_manager_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {

FakeSessionManagerClient::FakeSessionManagerClient()
    : first_boot_(false),
      start_device_wipe_call_count_(0),
      notify_lock_screen_shown_call_count_(0),
      notify_lock_screen_dismissed_call_count_(0) {
}

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

bool FakeSessionManagerClient::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void FakeSessionManagerClient::EmitLoginPromptVisible() {
}

void FakeSessionManagerClient::RestartJob(int pid,
                                          const std::string& command_line) {
}

void FakeSessionManagerClient::StartSession(const std::string& user_email) {
  DCHECK_EQ(0UL, user_sessions_.count(user_email));
  std::string user_id_hash =
      CryptohomeClient::GetStubSanitizedUsername(user_email);
  user_sessions_[user_email] = user_id_hash;
}

void FakeSessionManagerClient::StopSession() {
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
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, user_sessions_, true));
}

void FakeSessionManagerClient::RetrieveDevicePolicy(
    const RetrievePolicyCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, device_policy_));
}

void FakeSessionManagerClient::RetrievePolicyForUser(
    const std::string& username,
    const RetrievePolicyCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, user_policies_[username]));
}

std::string FakeSessionManagerClient::BlockingRetrievePolicyForUser(
    const std::string& username) {
  return user_policies_[username];
}

void FakeSessionManagerClient::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    const RetrievePolicyCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, device_local_account_policy_[account_id]));
}

void FakeSessionManagerClient::StoreDevicePolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_policy_ = policy_blob;
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
  FOR_EACH_OBSERVER(Observer, observers_, PropertyChangeComplete(true));
}

void FakeSessionManagerClient::StorePolicyForUser(
    const std::string& username,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  user_policies_[username] = policy_blob;
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
}

void FakeSessionManagerClient::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_local_account_policy_[account_id] = policy_blob;
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
}

void FakeSessionManagerClient::SetFlagsForUser(
    const std::string& username,
    const std::vector<std::string>& flags) {
}

void FakeSessionManagerClient::GetServerBackedStateKeys(
    const StateKeysCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, server_backed_state_keys_, first_boot_));
}

const std::string& FakeSessionManagerClient::device_policy() const {
  return device_policy_;
}

void FakeSessionManagerClient::set_device_policy(
    const std::string& policy_blob) {
  device_policy_ = policy_blob;
}

const std::string& FakeSessionManagerClient::user_policy(
    const std::string& username) const {
  std::map<std::string, std::string>::const_iterator it =
      user_policies_.find(username);
  return it == user_policies_.end() ? base::EmptyString() : it->second;
}

void FakeSessionManagerClient::set_user_policy(const std::string& username,
                                               const std::string& policy_blob) {
  user_policies_[username] = policy_blob;
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
