// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_local_account_policy_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chromeos/dbus/session_manager_client.h"

namespace em = enterprise_management;

namespace policy {

DeviceLocalAccountPolicyBroker::DeviceLocalAccountPolicyBroker(
    const std::string& account_id,
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service,
    Delegate* delegate)
    : account_id_(account_id),
      session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      status_(CloudPolicyStore::STATUS_OK),
      validation_status_(CloudPolicyValidatorBase::VALIDATION_OK) {}

DeviceLocalAccountPolicyBroker::~DeviceLocalAccountPolicyBroker() {}

void DeviceLocalAccountPolicyBroker::Load() {
  weak_factory_.InvalidateWeakPtrs();
  session_manager_client_->RetrieveDeviceLocalAccountPolicy(
      account_id_,
      base::Bind(&DeviceLocalAccountPolicyBroker::ValidateLoadedPolicyBlob,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyBroker::Store(
    const em::PolicyFetchResponse& policy) {
  weak_factory_.InvalidateWeakPtrs();
  CheckKeyAndValidate(
      make_scoped_ptr(new em::PolicyFetchResponse(policy)),
      base::Bind(&DeviceLocalAccountPolicyBroker::StoreValidatedPolicy,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyBroker::ValidateLoadedPolicyBlob(
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    status_ = CloudPolicyStore::STATUS_LOAD_ERROR;
    delegate_->OnLoadComplete(account_id_);
  } else {
    scoped_ptr<em::PolicyFetchResponse> policy(new em::PolicyFetchResponse());
    if (policy->ParseFromString(policy_blob)) {
      CheckKeyAndValidate(
          policy.Pass(),
          base::Bind(&DeviceLocalAccountPolicyBroker::UpdatePolicy,
                     weak_factory_.GetWeakPtr()));
    } else {
      status_ = CloudPolicyStore::STATUS_PARSE_ERROR;
      delegate_->OnLoadComplete(account_id_);
    }
  }
}

void DeviceLocalAccountPolicyBroker::UpdatePolicy(
    UserCloudPolicyValidator* validator) {
  if (validator->success()) {
    status_ = CloudPolicyStore::STATUS_OK;
    validation_status_ = CloudPolicyValidatorBase::VALIDATION_OK;
    policy_data_ = validator->policy_data().Pass();
    policy_settings_ = validator->payload().Pass();
  } else {
    status_ = CloudPolicyStore::STATUS_VALIDATION_ERROR;
    validation_status_ = validator->status();
  }
  delegate_->OnLoadComplete(account_id_);
}

void DeviceLocalAccountPolicyBroker::StoreValidatedPolicy(
    UserCloudPolicyValidator* validator) {
  if (!validator->success()) {
    status_ = CloudPolicyStore::STATUS_VALIDATION_ERROR;
    validation_status_ = validator->status();
    delegate_->OnLoadComplete(account_id_);
    return;
  }

  std::string policy_blob;
  if (!validator->policy()->SerializeToString(&policy_blob)) {
    status_ = CloudPolicyStore::STATUS_SERIALIZE_ERROR;
    delegate_->OnLoadComplete(account_id_);
    return;
  }

  session_manager_client_->StoreDeviceLocalAccountPolicy(
      account_id_,
      policy_blob,
      base::Bind(&DeviceLocalAccountPolicyBroker::HandleStoreResult,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyBroker::HandleStoreResult(bool success) {
  if (!success) {
    status_ = CloudPolicyStore::STATUS_STORE_ERROR;
    delegate_->OnLoadComplete(account_id_);
  } else {
    Load();
  }
}

void DeviceLocalAccountPolicyBroker::CheckKeyAndValidate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  device_settings_service_->GetOwnershipStatusAsync(
      base::Bind(&DeviceLocalAccountPolicyBroker::Validate,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(policy.Pass()),
                 callback));
}

void DeviceLocalAccountPolicyBroker::Validate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback,
    chromeos::DeviceSettingsService::OwnershipStatus ownership_status,
    bool is_owner) {
  DCHECK_NE(chromeos::DeviceSettingsService::OWNERSHIP_UNKNOWN,
            ownership_status);
  chromeos::OwnerKey* key = device_settings_service_->GetOwnerKey();
  if (!key->public_key()) {
    status_ = CloudPolicyStore::STATUS_BAD_STATE;
    delegate_->OnLoadComplete(account_id_);
    return;
  }

  scoped_ptr<UserCloudPolicyValidator> validator(
      UserCloudPolicyValidator::Create(policy.Pass()));
  validator->ValidateUsername(account_id_);
  validator->ValidatePolicyType(dm_protocol::kChromePublicAccountPolicyType);
  validator->ValidateAgainstCurrentPolicy(policy_data_.get(), false);
  validator->ValidatePayload();
  validator->ValidateSignature(*key->public_key(), false);
  validator.release()->StartValidation(callback);
}

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service)
    : session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      device_management_service_(NULL) {
  device_settings_service_->AddObserver(this);
  DeviceSettingsUpdated();
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  device_settings_service_->RemoveObserver(this);

  STLDeleteContainerPairSecondPointers(policy_brokers_.begin(),
                                       policy_brokers_.end());
  policy_brokers_.clear();
}

void DeviceLocalAccountPolicyService::Initialize(
    DeviceManagementService* device_management_service) {
  DCHECK(!device_management_service_);
  device_management_service_ = device_management_service;
}

void DeviceLocalAccountPolicyService::Shutdown() {
  DCHECK(device_management_service_);
  device_management_service_ = NULL;
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForAccount(
        const std::string& account_id) {
  PolicyBrokerMap::iterator entry = policy_brokers_.find(account_id);
  if (entry == policy_brokers_.end())
    return NULL;

  if (!entry->second) {
    entry->second = new DeviceLocalAccountPolicyBroker(account_id,
                                                       session_manager_client_,
                                                       device_settings_service_,
                                                       this);
  }

  return entry->second;
}

void DeviceLocalAccountPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceLocalAccountPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceLocalAccountPolicyService::OwnershipStatusChanged() {
  // TODO(mnissler): The policy key has changed, re-fetch policy. For
  // consumer devices, re-sign the current settings and send updates to
  // session_manager.
}

void DeviceLocalAccountPolicyService::DeviceSettingsUpdated() {
  const em::ChromeDeviceSettingsProto* device_settings =
      device_settings_service_->device_settings();
  if (device_settings)
    UpdateAccountList(*device_settings);
}

void DeviceLocalAccountPolicyService::UpdateAccountList(
    const em::ChromeDeviceSettingsProto& device_settings) {
  using google::protobuf::RepeatedPtrField;

  // Update |policy_brokers_|, keeping existing entries.
  PolicyBrokerMap new_policy_brokers;
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      device_settings.device_local_accounts().account();
  RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
  for (entry = accounts.begin(); entry != accounts.end(); ++entry) {
    if (entry->has_id()) {
      DeviceLocalAccountPolicyBroker*& broker = policy_brokers_[entry->id()];
      new_policy_brokers[entry->id()] = broker;
      broker = NULL;
    }
  }
  policy_brokers_.swap(new_policy_brokers);
  STLDeleteContainerPairSecondPointers(new_policy_brokers.begin(),
                                       new_policy_brokers.end());

  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceLocalAccountsChanged());

  // TODO(mnissler): Trigger policy fetches for all the new brokers on
  // cloud-managed devices.
}

void DeviceLocalAccountPolicyService::OnLoadComplete(
    const std::string& account_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnPolicyChanged(account_id));
}

}  // namespace policy
