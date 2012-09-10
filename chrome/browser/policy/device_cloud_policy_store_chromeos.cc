// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"

#include "base/bind.h"
#include "chrome/browser/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceCloudPolicyStoreChromeOS::DeviceCloudPolicyStoreChromeOS(
    chromeos::DeviceSettingsService* device_settings_service,
    EnterpriseInstallAttributes* install_attributes)
    : device_settings_service_(device_settings_service),
      install_attributes_(install_attributes),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  device_settings_service_->AddObserver(this);
}

DeviceCloudPolicyStoreChromeOS::~DeviceCloudPolicyStoreChromeOS() {
  device_settings_service_->RemoveObserver(this);
}

void DeviceCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  scoped_refptr<chromeos::OwnerKey> owner_key(
      device_settings_service_->GetOwnerKey());
  if (!install_attributes_->IsEnterpriseDevice() ||
      !device_settings_service_->policy_data() ||
      !owner_key || !owner_key->public_key()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  scoped_ptr<DeviceCloudPolicyValidator> validator(CreateValidator(policy));
  validator->ValidateSignature(*owner_key->public_key(), true);
  validator->ValidateAgainstCurrentPolicy(
      device_settings_service_->policy_data(), false);
  validator.release()->StartValidation();
}

void DeviceCloudPolicyStoreChromeOS::Load() {
  device_settings_service_->Load();
}

void DeviceCloudPolicyStoreChromeOS::RemoveStoredPolicy() {
  // Device policy cannot and should not be removed on Chrome OS.
  NOTREACHED();
}

void DeviceCloudPolicyStoreChromeOS::InstallInitialPolicy(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  if (!install_attributes_->IsEnterpriseDevice() &&
      device_settings_service_->status() !=
          chromeos::DeviceSettingsService::STORE_NO_POLICY) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  scoped_ptr<DeviceCloudPolicyValidator> validator(CreateValidator(policy));
  validator->ValidateInitialKey();
  validator.release()->StartValidation();
}

void DeviceCloudPolicyStoreChromeOS::OwnershipStatusChanged() {
  // Nothing to do.
}

void DeviceCloudPolicyStoreChromeOS::DeviceSettingsUpdated() {
  if (!weak_factory_.HasWeakPtrs())
    UpdateFromService();
}

scoped_ptr<DeviceCloudPolicyValidator>
    DeviceCloudPolicyStoreChromeOS::CreateValidator(
        const em::PolicyFetchResponse& policy) {
  scoped_ptr<DeviceCloudPolicyValidator> validator(
      DeviceCloudPolicyValidator::Create(
          scoped_ptr<em::PolicyFetchResponse>(
              new em::PolicyFetchResponse(policy)),
          base::Bind(&DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr())));
  validator->ValidateDomain(install_attributes_->GetDomain());
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  return validator.Pass();
}

void DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated(
    DeviceCloudPolicyValidator* validator) {
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    validation_status_ = validator->status();
    NotifyStoreError();
    return;
  }

  device_settings_service_->Store(
      validator->policy().Pass(),
      base::Bind(&DeviceCloudPolicyStoreChromeOS::OnPolicyStored,
                 weak_factory_.GetWeakPtr()));
}

void DeviceCloudPolicyStoreChromeOS::OnPolicyStored() {
  UpdateFromService();
}

void DeviceCloudPolicyStoreChromeOS::UpdateFromService() {
  if (!install_attributes_->IsEnterpriseDevice()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  switch (device_settings_service_->status()) {
    case chromeos::DeviceSettingsService::STORE_SUCCESS: {
      status_ = STATUS_OK;
      policy_.reset(new em::PolicyData());
      if (device_settings_service_->policy_data())
        policy_->MergeFrom(*device_settings_service_->policy_data());

      PolicyMap new_policy_map;
      if (is_managed()) {
        DecodeDevicePolicy(*device_settings_service_->device_settings(),
                           &new_policy_map, install_attributes_);
      }
      policy_map_.Swap(&new_policy_map);

      NotifyStoreLoaded();
      return;
    }
    case chromeos::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      status_ = STATUS_BAD_STATE;
      break;
    case chromeos::DeviceSettingsService::STORE_POLICY_ERROR:
    case chromeos::DeviceSettingsService::STORE_OPERATION_FAILED:
      status_ = STATUS_STORE_ERROR;
      break;
    case chromeos::DeviceSettingsService::STORE_NO_POLICY:
    case chromeos::DeviceSettingsService::STORE_INVALID_POLICY:
    case chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR:
      status_ = STATUS_LOAD_ERROR;
      break;
  }

  NotifyStoreError();
}

}  // namespace policy
