// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

DeviceSettingsTestHelper::DeviceSettingsTestHelper() {}

DeviceSettingsTestHelper::~DeviceSettingsTestHelper() {}

void DeviceSettingsTestHelper::FlushLoops() {
  // DeviceSettingsService may trigger operations that hop back and forth
  // between the message loop and the blocking pool. 2 iterations are currently
  // sufficient (key loading, signing).
  for (int i = 0; i < 2; ++i) {
    base::MessageLoop::current()->RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }
  base::MessageLoop::current()->RunUntilIdle();
}

void DeviceSettingsTestHelper::FlushStore() {
  std::vector<StorePolicyCallback> callbacks;
  callbacks.swap(device_policy_.store_callbacks_);
  for (std::vector<StorePolicyCallback>::iterator cb(callbacks.begin());
       cb != callbacks.end(); ++cb) {
    cb->Run(device_policy_.store_result_);
  }

  std::map<std::string, PolicyState>::iterator device_local_account_state;
  for (device_local_account_state = device_local_account_policy_.begin();
       device_local_account_state != device_local_account_policy_.end();
       ++device_local_account_state) {
    callbacks.swap(device_local_account_state->second.store_callbacks_);
    for (std::vector<StorePolicyCallback>::iterator cb(callbacks.begin());
         cb != callbacks.end(); ++cb) {
      cb->Run(device_local_account_state->second.store_result_);
    }
  }
}

void DeviceSettingsTestHelper::FlushRetrieve() {
  std::vector<RetrievePolicyCallback> callbacks;
  callbacks.swap(device_policy_.retrieve_callbacks_);
  for (std::vector<RetrievePolicyCallback>::iterator cb(callbacks.begin());
       cb != callbacks.end(); ++cb) {
    cb->Run(device_policy_.policy_blob_);
  }

  std::map<std::string, PolicyState>::iterator device_local_account_state;
  for (device_local_account_state = device_local_account_policy_.begin();
       device_local_account_state != device_local_account_policy_.end();
       ++device_local_account_state) {
    std::vector<RetrievePolicyCallback> callbacks;
    callbacks.swap(device_local_account_state->second.retrieve_callbacks_);
    for (std::vector<RetrievePolicyCallback>::iterator cb(callbacks.begin());
         cb != callbacks.end(); ++cb) {
      cb->Run(device_local_account_state->second.policy_blob_);
    }
  }
}

void DeviceSettingsTestHelper::Flush() {
  do {
    FlushLoops();
    FlushStore();
    FlushLoops();
    FlushRetrieve();
    FlushLoops();
  } while (HasPendingOperations());
}

bool DeviceSettingsTestHelper::HasPendingOperations() const {
  if (device_policy_.HasPendingOperations())
    return true;

  std::map<std::string, PolicyState>::const_iterator device_local_account_state;
  for (device_local_account_state = device_local_account_policy_.begin();
       device_local_account_state != device_local_account_policy_.end();
       ++device_local_account_state) {
    if (device_local_account_state->second.HasPendingOperations())
      return true;
  }

  return false;
}

void DeviceSettingsTestHelper::Init(dbus::Bus* bus) {}

void DeviceSettingsTestHelper::SetStubDelegate(
    SessionManagerClient::StubDelegate* delegate) {}

void DeviceSettingsTestHelper::AddObserver(Observer* observer) {}

void DeviceSettingsTestHelper::RemoveObserver(Observer* observer) {}

bool DeviceSettingsTestHelper::HasObserver(Observer* observer) {
  return false;
}

void DeviceSettingsTestHelper::EmitLoginPromptVisible() {}

void DeviceSettingsTestHelper::RestartJob(int pid,
                                          const std::string& command_line) {}

void DeviceSettingsTestHelper::StartSession(const std::string& user_email) {}

void DeviceSettingsTestHelper::StopSession() {}

void DeviceSettingsTestHelper::StartDeviceWipe() {}

void DeviceSettingsTestHelper::RequestLockScreen() {}

void DeviceSettingsTestHelper::NotifyLockScreenShown() {}

void DeviceSettingsTestHelper::NotifyLockScreenDismissed() {}

void DeviceSettingsTestHelper::RetrieveActiveSessions(
      const ActiveSessionsCallback& callback) {}

void DeviceSettingsTestHelper::RetrieveDevicePolicy(
    const RetrievePolicyCallback& callback) {
  device_policy_.retrieve_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::RetrievePolicyForUser(
    const std::string& username,
    const RetrievePolicyCallback& callback) {
}

std::string DeviceSettingsTestHelper::BlockingRetrievePolicyForUser(
    const std::string& username) {
  return "";
}

void DeviceSettingsTestHelper::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    const RetrievePolicyCallback& callback) {
  device_local_account_policy_[account_id].retrieve_callbacks_.push_back(
      callback);
}

void DeviceSettingsTestHelper::StoreDevicePolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_policy_.policy_blob_ = policy_blob;
  device_policy_.store_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::StorePolicyForUser(
    const std::string& username,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
}

void DeviceSettingsTestHelper::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_local_account_policy_[account_id].policy_blob_ = policy_blob;
  device_local_account_policy_[account_id].store_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::SetFlagsForUser(
    const std::string& account_id,
    const std::vector<std::string>& flags) {}

void DeviceSettingsTestHelper::GetServerBackedStateKeys(
    const StateKeysCallback& callback) {}

DeviceSettingsTestHelper::PolicyState::PolicyState()
    : store_result_(true) {}

DeviceSettingsTestHelper::PolicyState::~PolicyState() {}

ScopedDeviceSettingsTestHelper::ScopedDeviceSettingsTestHelper() {
  DeviceSettingsService::Initialize();
  DeviceSettingsService::Get()->SetSessionManager(this, new MockOwnerKeyUtil());
  DeviceSettingsService::Get()->Load();
  Flush();
}

ScopedDeviceSettingsTestHelper::~ScopedDeviceSettingsTestHelper() {
  Flush();
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

DeviceSettingsTestBase::DeviceSettingsTestBase()
    : user_manager_(new FakeUserManager()),
      user_manager_enabler_(user_manager_),
      owner_key_util_(new MockOwnerKeyUtil()),
      fake_dbus_thread_manager_(new FakeDBusThreadManager()) {
}

DeviceSettingsTestBase::~DeviceSettingsTestBase() {
  base::RunLoop().RunUntilIdle();
}

void DeviceSettingsTestBase::SetUp() {
  // Initialize DBusThreadManager with a stub implementation.
  chromeos::DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager_);

  base::RunLoop().RunUntilIdle();

  device_policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(
      false);
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_service_.SetSessionManager(&device_settings_test_helper_,
                                             owner_key_util_);
  OwnerSettingsService::SetOwnerKeyUtilForTesting(owner_key_util_.get());
  OwnerSettingsService::SetDeviceSettingsServiceForTesting(
      &device_settings_service_);
  profile_.reset(new TestingProfile());
}

void DeviceSettingsTestBase::TearDown() {
  OwnerSettingsService::SetOwnerKeyUtilForTesting(NULL);
  OwnerSettingsService::SetDeviceSettingsServiceForTesting(NULL);
  FlushDeviceSettings();
  device_settings_service_.UnsetSessionManager();
  DBusThreadManager::Shutdown();
}

void DeviceSettingsTestBase::FlushDeviceSettings() {
  device_settings_test_helper_.Flush();
}

void DeviceSettingsTestBase::ReloadDeviceSettings() {
  device_settings_service_.OwnerKeySet(true);
  FlushDeviceSettings();
}

void DeviceSettingsTestBase::InitOwner(const std::string& user_id,
                                       bool tpm_is_ready) {
  const User* user = user_manager_->FindUser(user_id);
  if (!user) {
    user = user_manager_->AddUser(user_id);
    profile_->set_profile_name(user_id);

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile_.get());
  }
  OwnerSettingsService* service =
      OwnerSettingsServiceFactory::GetForProfile(profile_.get());
  CHECK(service);
  if (tpm_is_ready)
    service->OnTPMTokenReady();
  OwnerSettingsServiceFactory::GetInstance()->SetUsername(user_id);
}

}  // namespace chromeos
