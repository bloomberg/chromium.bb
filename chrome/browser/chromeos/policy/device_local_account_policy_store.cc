// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_store.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/cloud/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/policy_types.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "policy/policy_constants.h"
#include "policy/proto/cloud_policy.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceLocalAccountPolicyStore::DeviceLocalAccountPolicyStore(
    const std::string& account_id,
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service)
    : account_id_(account_id),
      session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

DeviceLocalAccountPolicyStore::~DeviceLocalAccountPolicyStore() {}

void DeviceLocalAccountPolicyStore::Load() {
  weak_factory_.InvalidateWeakPtrs();
  session_manager_client_->RetrieveDeviceLocalAccountPolicy(
      account_id_,
      base::Bind(&DeviceLocalAccountPolicyStore::ValidateLoadedPolicyBlob,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::Store(
    const em::PolicyFetchResponse& policy) {
  weak_factory_.InvalidateWeakPtrs();
  CheckKeyAndValidate(
      make_scoped_ptr(new em::PolicyFetchResponse(policy)),
      base::Bind(&DeviceLocalAccountPolicyStore::StoreValidatedPolicy,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::ValidateLoadedPolicyBlob(
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    status_ = CloudPolicyStore::STATUS_LOAD_ERROR;
    NotifyStoreError();
  } else {
    scoped_ptr<em::PolicyFetchResponse> policy(new em::PolicyFetchResponse());
    if (policy->ParseFromString(policy_blob)) {
      CheckKeyAndValidate(
          policy.Pass(),
          base::Bind(&DeviceLocalAccountPolicyStore::UpdatePolicy,
                     weak_factory_.GetWeakPtr()));
    } else {
      status_ = CloudPolicyStore::STATUS_PARSE_ERROR;
      NotifyStoreError();
    }
  }
}

void DeviceLocalAccountPolicyStore::UpdatePolicy(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  // Exit the session when the lid is closed. The default behavior is to
  // suspend while leaving the session running, which is not desirable for
  // public sessions.
  policy_map_.Set(key::kLidCloseAction,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  base::Value::CreateIntegerValue(
                      chromeos::PowerPolicyController::ACTION_STOP_SESSION));

  // Force the |ShelfAutoHideBehavior| policy to |Never|, ensuring that the ash
  // shelf does not auto-hide.
  policy_map_.Set(key::kShelfAutoHideBehavior,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  Value::CreateStringValue("Never"));
  // Force the |ShowLogoutButtonInTray| policy to |true|, ensuring that a big,
  // red logout button is shown in the ash system tray.
  policy_map_.Set(key::kShowLogoutButtonInTray,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  Value::CreateBooleanValue(true));
  // Restrict device-local accounts to hosted apps for now (i.e. no extensions,
  // packaged apps etc.) for security/privacy reasons (i.e. we'd like to
  // prevent the admin from stealing private information from random people).
  scoped_ptr<base::ListValue> allowed_extension_types(new base::ListValue());
  allowed_extension_types->AppendString("hosted_app");
  policy_map_.Set(key::kExtensionAllowedTypes,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  allowed_extension_types.release());

  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

void DeviceLocalAccountPolicyStore::StoreValidatedPolicy(
    UserCloudPolicyValidator* validator) {
  if (!validator->success()) {
    status_ = CloudPolicyStore::STATUS_VALIDATION_ERROR;
    validation_status_ = validator->status();
    NotifyStoreError();
    return;
  }

  std::string policy_blob;
  if (!validator->policy()->SerializeToString(&policy_blob)) {
    status_ = CloudPolicyStore::STATUS_SERIALIZE_ERROR;
    NotifyStoreError();
    return;
  }

  session_manager_client_->StoreDeviceLocalAccountPolicy(
      account_id_,
      policy_blob,
      base::Bind(&DeviceLocalAccountPolicyStore::HandleStoreResult,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::HandleStoreResult(bool success) {
  if (!success) {
    status_ = CloudPolicyStore::STATUS_STORE_ERROR;
    NotifyStoreError();
  } else {
    Load();
  }
}

void DeviceLocalAccountPolicyStore::CheckKeyAndValidate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  device_settings_service_->GetOwnershipStatusAsync(
      base::Bind(&DeviceLocalAccountPolicyStore::Validate,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&policy),
                 callback));
}

void DeviceLocalAccountPolicyStore::Validate(
    scoped_ptr<em::PolicyFetchResponse> policy_response,
    const UserCloudPolicyValidator::CompletionCallback& callback,
    chromeos::DeviceSettingsService::OwnershipStatus ownership_status,
    bool is_owner) {
  DCHECK_NE(chromeos::DeviceSettingsService::OWNERSHIP_UNKNOWN,
            ownership_status);
  chromeos::OwnerKey* key = device_settings_service_->GetOwnerKey();
  if (!key || !key->public_key()) {
    status_ = CloudPolicyStore::STATUS_BAD_STATE;
    NotifyStoreLoaded();
    return;
  }

  scoped_ptr<UserCloudPolicyValidator> validator(
      UserCloudPolicyValidator::Create(policy_response.Pass()));
  validator->ValidateUsername(account_id_);
  validator->ValidatePolicyType(dm_protocol::kChromePublicAccountPolicyType);
  validator->ValidateAgainstCurrentPolicy(
      policy(),
      CloudPolicyValidatorBase::TIMESTAMP_REQUIRED,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator->ValidatePayload();
  validator->ValidateSignature(*key->public_key(), false);
  validator.release()->StartValidation(callback);
}

}  // namespace policy
