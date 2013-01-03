// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enrollment_handler_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Retry for InstallAttrs initialization every 500ms.
const int kLockRetryIntervalMs = 500;
// Maximum time to retry InstallAttrs initialization before we give up.
const int kLockRetryTimeoutMs = 10 * 60 * 1000;  // 10 minutes.

}  // namespace

EnrollmentHandlerChromeOS::EnrollmentHandlerChromeOS(
    DeviceCloudPolicyStoreChromeOS* store,
    EnterpriseInstallAttributes* install_attributes,
    scoped_ptr<CloudPolicyClient> client,
    const std::string& auth_token,
    const std::string& client_id,
    bool is_auto_enrollment,
    const AllowedDeviceModes& allowed_device_modes,
    const EnrollmentCallback& completion_callback)
    : store_(store),
      install_attributes_(install_attributes),
      client_(client.Pass()),
      auth_token_(auth_token),
      client_id_(client_id),
      is_auto_enrollment_(is_auto_enrollment),
      allowed_device_modes_(allowed_device_modes),
      completion_callback_(completion_callback),
      device_mode_(DEVICE_MODE_NOT_SET),
      enrollment_step_(STEP_PENDING),
      lockbox_init_duration_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  CHECK(!client_->is_registered());
  CHECK_EQ(DM_STATUS_SUCCESS, client_->status());
  store_->AddObserver(this);
  client_->AddObserver(this);
}

EnrollmentHandlerChromeOS::~EnrollmentHandlerChromeOS() {
  Stop();
  store_->RemoveObserver(this);
}

void EnrollmentHandlerChromeOS::StartEnrollment() {
  CHECK_EQ(STEP_PENDING, enrollment_step_);
  enrollment_step_ = STEP_LOADING_STORE;
  AttemptRegistration();
}

scoped_ptr<CloudPolicyClient> EnrollmentHandlerChromeOS::ReleaseClient() {
  Stop();
  return client_.Pass();
}

void EnrollmentHandlerChromeOS::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);
  CHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);

  enrollment_step_ = STEP_VALIDATION;

  // Validate the policy.
  scoped_ptr<DeviceCloudPolicyValidator> validator(
      DeviceCloudPolicyValidator::Create(
          scoped_ptr<em::PolicyFetchResponse>(
              new em::PolicyFetchResponse(*client_->policy()))));

  validator->ValidateTimestamp(base::Time(), base::Time::NowFromSystemTime(),
                               false);
  if (install_attributes_->IsEnterpriseDevice())
    validator->ValidateDomain(install_attributes_->GetDomain());
  validator->ValidateDMToken(client->dm_token());
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  validator->ValidateInitialKey();
  validator.release()->StartValidation(
      base::Bind(&EnrollmentHandlerChromeOS::PolicyValidated,
                 weak_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_REGISTRATION && client_->is_registered()) {
    enrollment_step_ = STEP_POLICY_FETCH,
    device_mode_ = client_->device_mode();
    if (device_mode_ == DEVICE_MODE_NOT_SET)
      device_mode_ = DEVICE_MODE_ENTERPRISE;
    if (!allowed_device_modes_.test(device_mode_)) {
      LOG(ERROR) << "Bad device mode " << device_mode_;
      ReportResult(EnrollmentStatus::ForStatus(
          EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE));
      return;
    }
    client_->FetchPolicy();
  } else {
    LOG(FATAL) << "Registration state changed to " << client_->is_registered()
               << " in step " << enrollment_step_;
  }
}

void EnrollmentHandlerChromeOS::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ < STEP_POLICY_FETCH)
    ReportResult(EnrollmentStatus::ForRegistrationError(client_->status()));
  else
    ReportResult(EnrollmentStatus::ForFetchError(client_->status()));
}

void EnrollmentHandlerChromeOS::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ == STEP_LOADING_STORE) {
    AttemptRegistration();
  } else if (enrollment_step_ == STEP_STORE_POLICY) {
    ReportResult(EnrollmentStatus::ForStatus(EnrollmentStatus::STATUS_SUCCESS));
  }
}

void EnrollmentHandlerChromeOS::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);
  ReportResult(EnrollmentStatus::ForStoreError(store_->status(),
                                               store_->validation_status()));
}

void EnrollmentHandlerChromeOS::AttemptRegistration() {
  CHECK_EQ(STEP_LOADING_STORE, enrollment_step_);
  if (store_->is_initialized()) {
    enrollment_step_ = STEP_REGISTRATION;
    client_->Register(auth_token_, client_id_, is_auto_enrollment_);
  }
}

void EnrollmentHandlerChromeOS::PolicyValidated(
    DeviceCloudPolicyValidator* validator) {
  CHECK_EQ(STEP_VALIDATION, enrollment_step_);
  if (validator->success()) {
    policy_ = validator->policy().Pass();
    enrollment_step_ = STEP_LOCK_DEVICE;
    WriteInstallAttributes(validator->policy_data()->username(), device_mode_,
                           validator->policy_data()->device_id());
  } else {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
  }
}

void EnrollmentHandlerChromeOS::WriteInstallAttributes(
    const std::string& user,
    DeviceMode device_mode,
    const std::string& device_id) {
  CHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  // Since this method is also called directly.
  weak_factory_.InvalidateWeakPtrs();

  EnterpriseInstallAttributes::LockResult lock_result =
      install_attributes_->LockDevice(user, device_mode, device_id);
  switch (lock_result) {
    case EnterpriseInstallAttributes::LOCK_SUCCESS:
      enrollment_step_ = STEP_STORE_POLICY;
      store_->InstallInitialPolicy(*policy_);
      return;
    case EnterpriseInstallAttributes::LOCK_NOT_READY:
      // We wait up to |kLockRetryTimeoutMs| milliseconds and if it hasn't
      // succeeded by then show an error to the user and stop the enrollment.
      if (lockbox_init_duration_ < kLockRetryTimeoutMs) {
        // InstallAttributes not ready yet, retry later.
        LOG(WARNING) << "Install Attributes not ready yet will retry in "
                     << kLockRetryIntervalMs << "ms.";
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&EnrollmentHandlerChromeOS::WriteInstallAttributes,
                       weak_factory_.GetWeakPtr(),
                       user, device_mode, device_id),
            base::TimeDelta::FromMilliseconds(kLockRetryIntervalMs));
        lockbox_init_duration_ += kLockRetryIntervalMs;
      } else {
        ReportResult(EnrollmentStatus::ForStatus(
            EnrollmentStatus::STATUS_LOCK_TIMEOUT));
      }
      return;
    case EnterpriseInstallAttributes::LOCK_BACKEND_ERROR:
      ReportResult(EnrollmentStatus::ForStatus(
          EnrollmentStatus::STATUS_LOCK_ERROR));
      return;
    case EnterpriseInstallAttributes::LOCK_WRONG_USER:
      LOG(ERROR) << "Enrollment cannot proceed because the InstallAttrs "
                 << "has been locked already!";
      ReportResult(EnrollmentStatus::ForStatus(
          EnrollmentStatus::STATUS_LOCK_WRONG_USER));
      return;
  }

  NOTREACHED() << "Invalid lock result " << lock_result;
  ReportResult(EnrollmentStatus::ForStatus(
      EnrollmentStatus::STATUS_LOCK_ERROR));
}

void EnrollmentHandlerChromeOS::Stop() {
  if (client_.get())
    client_->RemoveObserver(this);
  enrollment_step_ = STEP_FINISHED;
  weak_factory_.InvalidateWeakPtrs();
  completion_callback_.Reset();
}

void EnrollmentHandlerChromeOS::ReportResult(EnrollmentStatus status) {
  EnrollmentCallback callback = completion_callback_;
  Stop();

  if (status.status() != EnrollmentStatus::STATUS_SUCCESS) {
    LOG(WARNING) << "Enrollment failed: " << status.status()
                 << " " << status.client_status()
                 << " " << status.validation_status()
                 << " " << status.store_status();
  }

  if (!callback.is_null())
    callback.Run(status);
}

}  // namespace policy
