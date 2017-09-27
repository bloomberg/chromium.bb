// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using RetrievePolicyResponseType =
    chromeos::DeviceSettingsTestHelper::RetrievePolicyResponseType;

namespace chromeos {

DeviceSettingsTestHelper::DeviceSettingsTestHelper() {}

DeviceSettingsTestHelper::~DeviceSettingsTestHelper() {}

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
    cb->Run(device_policy_.policy_blob_, RetrievePolicyResponseType::SUCCESS);
  }

  std::map<std::string, PolicyState>::iterator device_local_account_state;
  for (device_local_account_state = device_local_account_policy_.begin();
       device_local_account_state != device_local_account_policy_.end();
       ++device_local_account_state) {
    std::vector<RetrievePolicyCallback> callbacks;
    callbacks.swap(device_local_account_state->second.retrieve_callbacks_);
    for (std::vector<RetrievePolicyCallback>::iterator cb(callbacks.begin());
         cb != callbacks.end(); ++cb) {
      cb->Run(device_local_account_state->second.policy_blob_,
              RetrievePolicyResponseType::SUCCESS);
    }
  }
}

void DeviceSettingsTestHelper::Flush() {
  do {
    content::RunAllTasksUntilIdle();
    FlushStore();
    content::RunAllTasksUntilIdle();
    FlushRetrieve();
    content::RunAllTasksUntilIdle();
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

void DeviceSettingsTestHelper::RetrieveDevicePolicy(
    const RetrievePolicyCallback& callback) {
  device_policy_.retrieve_callbacks_.push_back(callback);
}

RetrievePolicyResponseType
DeviceSettingsTestHelper::BlockingRetrieveDevicePolicy(
    std::string* policy_out) {
  *policy_out = device_policy_.policy_blob_;
  return RetrievePolicyResponseType::SUCCESS;
}

void DeviceSettingsTestHelper::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    const RetrievePolicyCallback& callback) {
  device_local_account_policy_[account_id].retrieve_callbacks_.push_back(
      callback);
}

RetrievePolicyResponseType
DeviceSettingsTestHelper::BlockingRetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    std::string* policy_out) {
  *policy_out = "";
  return RetrievePolicyResponseType::SUCCESS;
}

void DeviceSettingsTestHelper::StoreDevicePolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_policy_.policy_blob_ = policy_blob;
  device_policy_.store_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  device_local_account_policy_[account_id].policy_blob_ = policy_blob;
  device_local_account_policy_[account_id].store_callbacks_.push_back(callback);
}

DeviceSettingsTestHelper::PolicyState::PolicyState()
    : store_result_(true) {}

DeviceSettingsTestHelper::PolicyState::PolicyState(const PolicyState& other) =
    default;

DeviceSettingsTestHelper::PolicyState::~PolicyState() {}

ScopedDeviceSettingsTestHelper::ScopedDeviceSettingsTestHelper() {
  DeviceSettingsService::Initialize();
  DeviceSettingsService::Get()->SetSessionManager(
      this, new ownership::MockOwnerKeyUtil());
  DeviceSettingsService::Get()->Load();
  Flush();
}

ScopedDeviceSettingsTestHelper::~ScopedDeviceSettingsTestHelper() {
  Flush();
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

DeviceSettingsTestBase::DeviceSettingsTestBase()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      user_manager_(new FakeChromeUserManager()),
      user_manager_enabler_(user_manager_),
      owner_key_util_(new ownership::MockOwnerKeyUtil()) {
  OwnerSettingsServiceChromeOSFactory::SetDeviceSettingsServiceForTesting(
      &device_settings_service_);
  OwnerSettingsServiceChromeOSFactory::GetInstance()->SetOwnerKeyUtilForTesting(
      owner_key_util_);
}

DeviceSettingsTestBase::~DeviceSettingsTestBase() {
  base::RunLoop().RunUntilIdle();
}

void DeviceSettingsTestBase::SetUp() {
  // Initialize DBusThreadManager with a stub implementation.
  dbus_setter_ = chromeos::DBusThreadManager::GetSetterForTesting();

  base::RunLoop().RunUntilIdle();

  device_policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(
      false);
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_service_.SetSessionManager(&device_settings_test_helper_,
                                             owner_key_util_);
  profile_.reset(new TestingProfile());
}

void DeviceSettingsTestBase::TearDown() {
  OwnerSettingsServiceChromeOSFactory::SetDeviceSettingsServiceForTesting(NULL);
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

void DeviceSettingsTestBase::InitOwner(const AccountId& account_id,
                                       bool tpm_is_ready) {
  const user_manager::User* user = user_manager_->FindUser(account_id);
  if (!user) {
    user = user_manager_->AddUser(account_id);
    profile_->set_profile_name(account_id.GetUserEmail());

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile_.get());
    ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        const_cast<user_manager::User*>(user));
  }
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  CHECK(service);
  if (tpm_is_ready)
    service->OnTPMTokenReady(true /* token is enabled */);
}

}  // namespace chromeos
