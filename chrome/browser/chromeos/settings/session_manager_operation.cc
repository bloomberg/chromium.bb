// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/session_manager_operation.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"

namespace em = enterprise_management;

namespace chromeos {

SessionManagerOperation::SessionManagerOperation(const Callback& callback)
    : session_manager_client_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      callback_(callback),
      force_key_load_(false),
      is_loading_(false) {}

SessionManagerOperation::~SessionManagerOperation() {}

void SessionManagerOperation::Start(
    SessionManagerClient* session_manager_client,
    scoped_refptr<OwnerKeyUtil> owner_key_util,
    scoped_refptr<OwnerKey> owner_key) {
  session_manager_client_ = session_manager_client;
  owner_key_util_ = owner_key_util;
  owner_key_ = owner_key;
  Run();
}

void SessionManagerOperation::RestartLoad(bool key_changed) {
  if (key_changed)
    owner_key_ = NULL;

  if (!is_loading_)
    return;

  // Abort previous load operations.
  weak_factory_.InvalidateWeakPtrs();
  StartLoading();
}

void SessionManagerOperation::StartLoading() {
  is_loading_ = true;
  EnsureOwnerKey(base::Bind(&SessionManagerOperation::RetrieveDeviceSettings,
                            weak_factory_.GetWeakPtr()));
}

void SessionManagerOperation::ReportResult(
    DeviceSettingsService::Status status) {
  callback_.Run(this, status);
}

void SessionManagerOperation::EnsureOwnerKey(const base::Closure& callback) {
  if (force_key_load_ || !owner_key_.get() || !owner_key_->public_key()) {
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&SessionManagerOperation::LoadOwnerKey,
                   owner_key_util_, owner_key_),
        base::Bind(&SignAndStoreSettingsOperation::StoreOwnerKey,
                   weak_factory_.GetWeakPtr(), callback));
  } else {
    callback.Run();
  }
}

// static
scoped_refptr<OwnerKey> SessionManagerOperation::LoadOwnerKey(
    scoped_refptr<OwnerKeyUtil> util,
    scoped_refptr<OwnerKey> current_key) {
  scoped_ptr<std::vector<uint8> > public_key;
  scoped_ptr<crypto::RSAPrivateKey> private_key;

  // Keep any already-existing keys.
  if (current_key.get()) {
    if (current_key->public_key())
      public_key.reset(new std::vector<uint8>(*current_key->public_key()));
    if (current_key->private_key())
      private_key.reset(current_key->private_key()->Copy());
  }

  if (!public_key.get() && util->IsPublicKeyPresent()) {
    public_key.reset(new std::vector<uint8>());
    if (!util->ImportPublicKey(public_key.get()))
      LOG(ERROR) << "Failed to load public owner key.";
  }

  if (public_key.get() && !private_key.get()) {
    private_key.reset(util->FindPrivateKey(*public_key));
    if (!private_key.get())
      VLOG(1) << "Failed to load private owner key.";
  }

  return new OwnerKey(public_key.Pass(), private_key.Pass());
}

void SessionManagerOperation::StoreOwnerKey(const base::Closure& callback,
                                            scoped_refptr<OwnerKey> new_key) {
  force_key_load_ = false;
  owner_key_ = new_key;

  if (!owner_key_.get() || !owner_key_->public_key()) {
    ReportResult(DeviceSettingsService::STORE_KEY_UNAVAILABLE);
    return;
  }

  callback.Run();
}

void SessionManagerOperation::RetrieveDeviceSettings() {
  session_manager_client()->RetrieveDevicePolicy(
      base::Bind(&SessionManagerOperation::ValidateDeviceSettings,
                 weak_factory_.GetWeakPtr()));
}

void SessionManagerOperation::ValidateDeviceSettings(
    const std::string& policy_blob) {
  scoped_ptr<em::PolicyFetchResponse> policy(new em::PolicyFetchResponse());
  if (policy_blob.empty()) {
    ReportResult(DeviceSettingsService::STORE_NO_POLICY);
    return;
  }

  if (!policy->ParseFromString(policy_blob) ||
      !policy->IsInitialized()) {
    ReportResult(DeviceSettingsService::STORE_INVALID_POLICY);
    return;
  }

  policy::DeviceCloudPolicyValidator* validator =
      policy::DeviceCloudPolicyValidator::Create(
          policy.Pass(),
          base::Bind(&SessionManagerOperation::ReportValidatorStatus,
                     weak_factory_.GetWeakPtr()));

  // Policy auto-generated by session manager doesn't include a timestamp, so we
  // need to allow missing timestamps.
  validator->ValidateAgainstCurrentPolicy(
      policy_data_.get(),
      !policy_data_.get() || !policy_data_->has_request_token());
  validator->ValidatePolicyType(policy::dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  validator->ValidateSignature(*owner_key_->public_key(), false);
  validator->StartValidation();
}

void SessionManagerOperation::ReportValidatorStatus(
    policy::DeviceCloudPolicyValidator* validator) {
  DeviceSettingsService::Status status =
      DeviceSettingsService::STORE_VALIDATION_ERROR;
  if (validator->success()) {
    status = DeviceSettingsService::STORE_SUCCESS;
    policy_data_ = validator->policy_data().Pass();
    device_settings_ = validator->payload().Pass();
  } else {
    LOG(ERROR) << "Policy validation failed: " << validator->status();
  }

  ReportResult(status);
}

LoadSettingsOperation::LoadSettingsOperation(const Callback& callback)
    : SessionManagerOperation(callback) {}

LoadSettingsOperation::~LoadSettingsOperation() {}

void LoadSettingsOperation::Run() {
  StartLoading();
}

StoreSettingsOperation::StoreSettingsOperation(
    const Callback& callback,
    scoped_ptr<em::PolicyFetchResponse> policy)
    : SessionManagerOperation(callback),
      policy_(policy.Pass()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

StoreSettingsOperation::~StoreSettingsOperation() {}

void StoreSettingsOperation::Run() {
  session_manager_client()->StoreDevicePolicy(
      policy_->SerializeAsString(),
      base::Bind(&StoreSettingsOperation::HandleStoreResult,
                 weak_factory_.GetWeakPtr()));
}

void StoreSettingsOperation::HandleStoreResult(bool success) {
  if (!success)
    ReportResult(DeviceSettingsService::STORE_OPERATION_FAILED);
  else
    StartLoading();
}

SignAndStoreSettingsOperation::SignAndStoreSettingsOperation(
    const Callback& callback,
    scoped_ptr<em::ChromeDeviceSettingsProto> new_settings,
    const std::string& username)
    : SessionManagerOperation(callback),
      new_settings_(new_settings.Pass()),
      username_(username),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(new_settings_.get());
}

SignAndStoreSettingsOperation::~SignAndStoreSettingsOperation() {}

void SignAndStoreSettingsOperation::Run() {
  EnsureOwnerKey(base::Bind(&SignAndStoreSettingsOperation::StartSigning,
                            weak_factory_.GetWeakPtr()));
}

void SignAndStoreSettingsOperation::StartSigning() {
  if (!owner_key() || !owner_key()->private_key() || username_.empty()) {
    ReportResult(DeviceSettingsService::STORE_KEY_UNAVAILABLE);
    return;
  }

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&SignAndStoreSettingsOperation::AssembleAndSignPolicy,
                 base::Passed(&new_settings_), username_, owner_key()),
      base::Bind(&SignAndStoreSettingsOperation::StoreDeviceSettingsBlob,
                 weak_factory_.GetWeakPtr()));
}

// static
std::string SignAndStoreSettingsOperation::AssembleAndSignPolicy(
    scoped_ptr<em::ChromeDeviceSettingsProto> device_settings,
    const std::string& username,
    scoped_refptr<OwnerKey> owner_key) {
  // Assemble the policy.
  em::PolicyFetchResponse policy_response;
  em::PolicyData policy;
  policy.set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
  policy.set_timestamp((base::Time::NowFromSystemTime() -
                        base::Time::UnixEpoch()).InMilliseconds());
  policy.set_username(username);
  if (!device_settings->SerializeToString(policy.mutable_policy_value()) ||
      !policy.SerializeToString(policy_response.mutable_policy_data())) {
    LOG(ERROR) << "Failed to encode policy payload.";
    return std::string();
  }

  // Generate the signature.
  scoped_ptr<crypto::SignatureCreator> signature_creator(
      crypto::SignatureCreator::Create(owner_key->private_key()));
  signature_creator->Update(
      reinterpret_cast<const uint8*>(policy_response.policy_data().c_str()),
      policy_response.policy_data().size());
  std::vector<uint8> signature_bytes;
  std::string policy_blob;
  if (!signature_creator->Final(&signature_bytes)) {
    LOG(ERROR) << "Failed to create policy signature.";
    return std::string();
  }

  policy_response.mutable_policy_data_signature()->assign(
      reinterpret_cast<const char*>(vector_as_array(&signature_bytes)),
      signature_bytes.size());
  return policy_response.SerializeAsString();
}

void SignAndStoreSettingsOperation::StoreDeviceSettingsBlob(
    std::string device_settings_blob) {
  if (device_settings_blob.empty()) {
    ReportResult(DeviceSettingsService::STORE_POLICY_ERROR);
    return;
  }

  session_manager_client()->StoreDevicePolicy(
      device_settings_blob,
      base::Bind(&SignAndStoreSettingsOperation::HandleStoreResult,
                 weak_factory_.GetWeakPtr()));
}

void SignAndStoreSettingsOperation::HandleStoreResult(bool success) {
  if (!success)
    ReportResult(DeviceSettingsService::STORE_OPERATION_FAILED);
  else
    StartLoading();
}

}  // namespace chromeos
