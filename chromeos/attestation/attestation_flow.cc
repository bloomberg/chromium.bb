// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow.h"

#include "base/bind.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace chromeos {
namespace attestation {

namespace {

// Redirects to one of three callbacks based on a boolean value and dbus call
// status.
//
// Parameters
//   on_true - Called when status=succes and value=true.
//   on_false - Called when status=success and value=false.
//   on_fail - Called when status=failure.
//   status - The D-Bus operation status.
//   value - The value returned by the D-Bus operation.
void DBusBoolRedirectCallback(const base::Closure& on_true,
                              const base::Closure& on_false,
                              const base::Closure& on_fail,
                              DBusMethodCallStatus status,
                              bool value) {
  if (status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Attestation: Failed to query enrollment state.";
    if (!on_fail.is_null())
      on_fail.Run();
    return;
  }
  const base::Closure& task = value ? on_true : on_false;
  if (!task.is_null())
    task.Run();
}

void DBusDataMethodCallback(
    const AttestationFlow::CertificateCallback& callback,
    DBusMethodCallStatus status,
    bool result,
    const std::string& data) {
  if (status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Attestation: DBus data operation failed.";
    if (!callback.is_null())
      callback.Run(false, "");
    return;
  }
  if (!callback.is_null())
    callback.Run(result, data);
}

AttestationKeyType GetKeyTypeForProfile(
    AttestationCertificateProfile profile) {
  switch (profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return KEY_DEVICE;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return KEY_USER;
  }
  NOTREACHED();
  return KEY_USER;
}

std::string GetKeyNameForProfile(AttestationCertificateProfile profile,
                                 const std::string& origin) {
  switch (profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return kEnterpriseMachineKey;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
      return kEnterpriseUserKey;
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return std::string(kContentProtectionKeyPrefix) + origin;
  }
  NOTREACHED();
  return "";
}

}  // namespace

AttestationFlow::AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                                 CryptohomeClient* cryptohome_client,
                                 scoped_ptr<ServerProxy> server_proxy)
    : async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      server_proxy_(server_proxy.Pass()),
      weak_factory_(this) {
}

AttestationFlow::~AttestationFlow() {
}

void AttestationFlow::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const std::string& user_id,
    const std::string& request_origin,
    bool force_new_key,
    const CertificateCallback& callback) {
  // If this device has not enrolled with the Privacy CA, we need to do that
  // first.  Once enrolled we can proceed with the certificate request.
  base::Closure do_cert_request = base::Bind(
      &AttestationFlow::StartCertificateRequest,
      weak_factory_.GetWeakPtr(),
      certificate_profile,
      user_id,
      request_origin,
      force_new_key,
      callback);
  base::Closure on_enroll_failure = base::Bind(callback, false, "");
  base::Closure do_enroll = base::Bind(&AttestationFlow::StartEnroll,
                                       weak_factory_.GetWeakPtr(),
                                       on_enroll_failure,
                                       do_cert_request);
  cryptohome_client_->TpmAttestationIsEnrolled(base::Bind(
      &DBusBoolRedirectCallback,
      do_cert_request,  // If enrolled, proceed with cert request.
      do_enroll,        // If not enrolled, initiate enrollment.
      on_enroll_failure));
}

void AttestationFlow::StartEnroll(const base::Closure& on_failure,
                                  const base::Closure& next_task) {
  // Get the attestation service to create a Privacy CA enrollment request.
  async_caller_->AsyncTpmAttestationCreateEnrollRequest(
      server_proxy_->GetType(),
      base::Bind(&AttestationFlow::SendEnrollRequestToPCA,
                 weak_factory_.GetWeakPtr(),
                 on_failure,
                 next_task));
}

void AttestationFlow::SendEnrollRequestToPCA(const base::Closure& on_failure,
                                             const base::Closure& next_task,
                                             bool success,
                                             const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create enroll request.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendEnrollRequest(
      data,
      base::Bind(&AttestationFlow::SendEnrollResponseToDaemon,
                 weak_factory_.GetWeakPtr(),
                 on_failure,
                 next_task));
}

void AttestationFlow::SendEnrollResponseToDaemon(
    const base::Closure& on_failure,
    const base::Closure& next_task,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Enroll request failed.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Forward the response to the attestation service to complete enrollment.
  async_caller_->AsyncTpmAttestationEnroll(
      server_proxy_->GetType(),
      data,
      base::Bind(&AttestationFlow::OnEnrollComplete,
                 weak_factory_.GetWeakPtr(),
                 on_failure,
                 next_task));
}

void AttestationFlow::OnEnrollComplete(const base::Closure& on_failure,
                                       const base::Closure& next_task,
                                       bool success,
                                       cryptohome::MountError /*not_used*/) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to complete enrollment.";
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }

  // Enrollment has successfully completed, we can move on to whatever is next.
  if (!next_task.is_null())
    next_task.Run();
}

void AttestationFlow::StartCertificateRequest(
    AttestationCertificateProfile certificate_profile,
    const std::string& user_id,
    const std::string& request_origin,
    bool generate_new_key,
    const CertificateCallback& callback) {
  AttestationKeyType key_type = GetKeyTypeForProfile(certificate_profile);
  std::string key_name = GetKeyNameForProfile(certificate_profile,
                                              request_origin);
  if (generate_new_key) {
    // Get the attestation service to create a Privacy CA certificate request.
    async_caller_->AsyncTpmAttestationCreateCertRequest(
        server_proxy_->GetType(),
        certificate_profile,
        user_id,
        request_origin,
        base::Bind(&AttestationFlow::SendCertificateRequestToPCA,
                   weak_factory_.GetWeakPtr(),
                   key_type,
                   user_id,
                   key_name,
                   callback));
  } else {
    // If the key already exists, query the existing certificate.
    base::Closure on_key_exists = base::Bind(
        &AttestationFlow::GetExistingCertificate,
        weak_factory_.GetWeakPtr(),
        key_type,
        user_id,
        key_name,
        callback);
    // If the key does not exist, call this method back with |generate_new_key|
    // set to true.
    base::Closure on_key_not_exists = base::Bind(
        &AttestationFlow::StartCertificateRequest,
        weak_factory_.GetWeakPtr(),
        certificate_profile,
        user_id,
        request_origin,
        true,
        callback);
    cryptohome_client_->TpmAttestationDoesKeyExist(
        key_type,
        user_id,
        key_name,
        base::Bind(&DBusBoolRedirectCallback,
            on_key_exists,
            on_key_not_exists,
            base::Bind(callback, false, "")));
  }
}

void AttestationFlow::SendCertificateRequestToPCA(
    AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const CertificateCallback& callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Failed to create certificate request.";
    if (!callback.is_null())
      callback.Run(false, "");
    return;
  }

  // Send the request to the Privacy CA.
  server_proxy_->SendCertificateRequest(
      data,
      base::Bind(&AttestationFlow::SendCertificateResponseToDaemon,
                 weak_factory_.GetWeakPtr(),
                 key_type,
                 user_id,
                 key_name,
                 callback));
}

void AttestationFlow::SendCertificateResponseToDaemon(
    AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const CertificateCallback& callback,
    bool success,
    const std::string& data) {
  if (!success) {
    LOG(ERROR) << "Attestation: Certificate request failed.";
    if (!callback.is_null())
      callback.Run(false, "");
    return;
  }

  // Forward the response to the attestation service to complete the operation.
  async_caller_->AsyncTpmAttestationFinishCertRequest(data,
                                                      key_type,
                                                      user_id,
                                                      key_name,
                                                      base::Bind(callback));
}

void AttestationFlow::GetExistingCertificate(
    AttestationKeyType key_type,
    const std::string& user_id,
    const std::string& key_name,
    const CertificateCallback& callback) {
  cryptohome_client_->TpmAttestationGetCertificate(
      key_type,
      user_id,
      key_name,
      base::Bind(&DBusDataMethodCallback, callback));
}

ServerProxy::~ServerProxy() {}

PrivacyCAType ServerProxy::GetType() {
  return DEFAULT_PCA;
}

}  // namespace attestation
}  // namespace chromeos
