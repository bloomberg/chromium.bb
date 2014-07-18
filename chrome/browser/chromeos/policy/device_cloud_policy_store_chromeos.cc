// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceCloudPolicyStoreChromeOS::DeviceCloudPolicyStoreChromeOS(
    chromeos::DeviceSettingsService* device_settings_service,
    EnterpriseInstallAttributes* install_attributes,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : device_settings_service_(device_settings_service),
      install_attributes_(install_attributes),
      background_task_runner_(background_task_runner),
      enrollment_validation_done_(false),
      weak_factory_(this) {
  device_settings_service_->AddObserver(this);
}

DeviceCloudPolicyStoreChromeOS::~DeviceCloudPolicyStoreChromeOS() {
  device_settings_service_->RemoveObserver(this);
}

void DeviceCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  scoped_refptr<chromeos::PublicKey> public_key(
      device_settings_service_->GetPublicKey());
  if (!install_attributes_->IsEnterpriseDevice() ||
      !device_settings_service_->policy_data() || !public_key.get() ||
      !public_key->is_loaded()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  scoped_ptr<DeviceCloudPolicyValidator> validator(CreateValidator(policy));
  validator->ValidateSignature(public_key->as_string(),
                               GetPolicyVerificationKey(),
                               install_attributes_->GetDomain(),
                               true);
  validator->ValidateAgainstCurrentPolicy(
      device_settings_service_->policy_data(),
      CloudPolicyValidatorBase::TIMESTAMP_REQUIRED,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator.release()->StartValidation(
      base::Bind(&DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                 weak_factory_.GetWeakPtr()));
}

void DeviceCloudPolicyStoreChromeOS::Load() {
  device_settings_service_->Load();
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
  validator->ValidateInitialKey(GetPolicyVerificationKey(),
                                install_attributes_->GetDomain());
  validator.release()->StartValidation(
      base::Bind(&DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                 weak_factory_.GetWeakPtr()));
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
          background_task_runner_));
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

  // Once per session, validate internal consistency of enrollment state (DM
  // token must be present on enrolled devices) and in case of failure set flag
  // to indicate that recovery is required.
  const chromeos::DeviceSettingsService::Status status =
      device_settings_service_->status();
  switch (status) {
    case chromeos::DeviceSettingsService::STORE_SUCCESS:
    case chromeos::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
    case chromeos::DeviceSettingsService::STORE_NO_POLICY:
    case chromeos::DeviceSettingsService::STORE_INVALID_POLICY:
    case chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR: {
      if (!enrollment_validation_done_) {
        enrollment_validation_done_ = true;
        const bool has_dm_token =
            status == chromeos::DeviceSettingsService::STORE_SUCCESS &&
            device_settings_service_->policy_data() &&
            device_settings_service_->policy_data()->has_request_token();

        // At the time LoginDisplayHostImpl decides whether enrollment flow is
        // to be started, policy hasn't been read yet.  To work around this,
        // once the need for recovery is detected upon policy load, a flag is
        // stored in prefs which is accessed by LoginDisplayHostImpl early
        // during (next) boot.
        if (!has_dm_token) {
          LOG(ERROR) << "Device policy read on enrolled device yields "
                     << "no DM token! Status: " << status << ".";
          chromeos::StartupUtils::MarkEnrollmentRecoveryRequired();
        }
        UMA_HISTOGRAM_BOOLEAN("Enterprise.EnrolledPolicyHasDMToken",
                              has_dm_token);
      }
      break;
    }
    case chromeos::DeviceSettingsService::STORE_POLICY_ERROR:
    case chromeos::DeviceSettingsService::STORE_OPERATION_FAILED:
    case chromeos::DeviceSettingsService::STORE_TEMP_VALIDATION_ERROR:
      // Do nothing for write errors or transient read errors.
      break;
  }

  switch (status) {
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
    case chromeos::DeviceSettingsService::STORE_TEMP_VALIDATION_ERROR:
      status_ = STATUS_LOAD_ERROR;
      break;
  }

  NotifyStoreError();
}

}  // namespace policy
