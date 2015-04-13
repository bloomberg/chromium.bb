// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_enroller_impl.h"

#include "base/bind.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_utils.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"

namespace proximity_auth {

namespace {

// A successful SetupEnrollment or FinishEnrollment response should contain this
// status string.
const char kResponseStatusOk[] = "OK";

// The name of the "gcmV1" protocol that the enrolling device supports.
const char kSupportedEnrollmentTypeGcmV1[] = "gcmV1";

// The version field of the GcmMetadata message.
const int kGCMMetadataVersion = 1;

// Returns true if |device_info| contains the required fields for enrollment.
bool ValidateDeviceInfo(const cryptauth::GcmDeviceInfo& device_info) {
  if (!device_info.has_user_public_key()) {
    VLOG(1) << "Expected user_public_key field in GcmDeviceInfo.";
    return false;
  }

  if (!device_info.has_key_handle()) {
    VLOG(1) << "Expected key_handle field in GcmDeviceInfo.";
    return false;
  }

  if (!device_info.has_long_device_id()) {
    VLOG(1) << "Expected long_device_id field in GcmDeviceInfo.";
    return false;
  }

  if (!device_info.has_device_type()) {
    VLOG(1) << "Expected device_type field in GcmDeviceInfo.";
    return false;
  }

  return true;
}

// Creates the public metadata to put in the SecureMessage that is sent to the
// server with the FinishEnrollment request.
std::string CreateEnrollmentPublicMetadata() {
  cryptauth::GcmMetadata metadata;
  metadata.set_version(kGCMMetadataVersion);
  metadata.set_type(cryptauth::MessageType::ENROLLMENT);
  return metadata.SerializeAsString();
}

}  // namespace

CryptAuthEnrollerImpl::CryptAuthEnrollerImpl(
    scoped_ptr<CryptAuthClientFactory> client_factory,
    scoped_ptr<SecureMessageDelegate> secure_message_delegate_)
    : client_factory_(client_factory.Pass()),
      secure_message_delegate_(secure_message_delegate_.Pass()),
      weak_ptr_factory_(this) {
}

CryptAuthEnrollerImpl::~CryptAuthEnrollerImpl() {
}

void CryptAuthEnrollerImpl::Enroll(
    const cryptauth::GcmDeviceInfo& device_info,
    cryptauth::InvocationReason invocation_reason,
    const EnrollmentFinishedCallback& callback) {
  if (!callback_.is_null()) {
    VLOG(1) << "Enroll() already called. Do not reuse.";
    callback.Run(false);
    return;
  }

  device_info_ = device_info;
  invocation_reason_ = invocation_reason;
  callback_ = callback;

  if (!ValidateDeviceInfo(device_info)) {
    callback.Run(false);
    return;
  }

  secure_message_delegate_->GenerateKeyPair(
      base::Bind(&CryptAuthEnrollerImpl::OnKeyPairGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnKeyPairGenerated(const std::string& public_key,
                                               const std::string& private_key) {
  session_public_key_ = public_key;
  session_private_key_ = private_key;

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth::SetupEnrollmentRequest request;
  request.add_types(kSupportedEnrollmentTypeGcmV1);
  request.set_invocation_reason(invocation_reason_);
  cryptauth_client_->SetupEnrollment(
      request, base::Bind(&CryptAuthEnrollerImpl::OnSetupEnrollmentSuccess,
                          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CryptAuthEnrollerImpl::OnSetupEnrollmentFailure,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnSetupEnrollmentSuccess(
    const cryptauth::SetupEnrollmentResponse& response) {
  if (response.status() != kResponseStatusOk) {
    VLOG(1) << "Unexpected status for SetupEnrollment: " << response.status();
    callback_.Run(false);
    return;
  }

  if (response.infos_size() == 0) {
    VLOG(1) << "No response info returned by server for SetupEnrollment";
    callback_.Run(false);
    return;
  }

  setup_info_ = response.infos(0);
  device_info_.set_enrollment_session_id(setup_info_.enrollment_session_id());
  secure_message_delegate_->DeriveKey(
      session_private_key_, setup_info_.server_ephemeral_key(),
      base::Bind(&CryptAuthEnrollerImpl::OnKeyDerived,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnSetupEnrollmentFailure(const std::string& error) {
  VLOG(1) << "SetupEnrollment API failed with error: " << error;
  callback_.Run(false);
}

void CryptAuthEnrollerImpl::OnKeyDerived(const std::string& symmetric_key) {
  symmetric_key_ = symmetric_key;
  SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = securemessage::NONE;
  options.signature_scheme = securemessage::ECDSA_P256_SHA256;
  options.verification_key_id = session_public_key_;

  // The inner message contains the signed device information that will be
  // sent to CryptAuth.
  secure_message_delegate_->CreateSecureMessage(
      device_info_.SerializeAsString(), session_private_key_, options,
      base::Bind(&CryptAuthEnrollerImpl::OnInnerSecureMessageCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnInnerSecureMessageCreated(
    const std::string& inner_message) {
  SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = securemessage::AES_256_CBC;
  options.signature_scheme = securemessage::HMAC_SHA256;
  options.public_metadata = CreateEnrollmentPublicMetadata();

  // The outer message encrypts and signs the inner message with the derived
  // symmetric session key.
  secure_message_delegate_->CreateSecureMessage(
      inner_message, symmetric_key_, options,
      base::Bind(&CryptAuthEnrollerImpl::OnOuterSecureMessageCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnOuterSecureMessageCreated(
    const std::string& outer_message) {
  cryptauth::FinishEnrollmentRequest request;
  request.set_enrollment_session_id(setup_info_.enrollment_session_id());
  request.set_enrollment_message(outer_message);
  request.set_device_ephemeral_key(session_public_key_);
  request.set_invocation_reason(invocation_reason_);

  cryptauth_client_ = client_factory_->CreateInstance();
  cryptauth_client_->FinishEnrollment(
      request, base::Bind(&CryptAuthEnrollerImpl::OnFinishEnrollmentSuccess,
                          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CryptAuthEnrollerImpl::OnFinishEnrollmentFailure,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptAuthEnrollerImpl::OnFinishEnrollmentSuccess(
    const cryptauth::FinishEnrollmentResponse& response) {
  if (response.status() != kResponseStatusOk) {
    VLOG(1) << "Unexpected status for FinishEnrollment: " << response.status();
    callback_.Run(false);
  } else {
    callback_.Run(true);
  }
}

void CryptAuthEnrollerImpl::OnFinishEnrollmentFailure(
    const std::string& error) {
  VLOG(1) << "FinishEnrollment API failed with error: " << error;
  callback_.Run(false);
}

}  // namespace proximity_auth
