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

}  // namespace

const char AttestationFlow::kEnterpriseMachineKey[] = "attest-ent-machine";

AttestationFlow::AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                                 CryptohomeClient* cryptohome_client,
                                 ServerProxy* server_proxy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      async_caller_(async_caller),
      cryptohome_client_(cryptohome_client),
      server_proxy_(server_proxy) {
}

AttestationFlow::~AttestationFlow() {
}

void AttestationFlow::GetCertificate(const std::string& name,
                                     const CertificateCallback& callback) {
  // If this device has not enrolled with the Privacy CA, we need to do that
  // first.  Once enrolled we can proceed with the certificate request.
  base::Closure do_cert_request = base::Bind(
      &AttestationFlow::StartCertificateRequest,
      weak_factory_.GetWeakPtr(),
      name,
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
  async_caller_->AsyncTpmAttestationCreateEnrollRequest(base::Bind(
      &AttestationFlow::SendEnrollRequestToPCA,
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
    const std::string& name,
    const CertificateCallback& callback) {
  // Get the attestation service to create a Privacy CA certificate request.
  int options = CryptohomeClient::INCLUDE_DEVICE_STATE;
  if (name == kEnterpriseMachineKey)
    options |= CryptohomeClient::INCLUDE_STABLE_ID;
  async_caller_->AsyncTpmAttestationCreateCertRequest(
      options,
      base::Bind(&AttestationFlow::SendCertificateRequestToPCA,
                 weak_factory_.GetWeakPtr(),
                 name,
                 callback));
}

void AttestationFlow::SendCertificateRequestToPCA(
    const std::string& name,
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
                 name,
                 callback));
}

void AttestationFlow::SendCertificateResponseToDaemon(
    const std::string& name,
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
  CryptohomeClient::AttestationKeyType key_type = CryptohomeClient::USER_KEY;
  if (name == kEnterpriseMachineKey)
    key_type = CryptohomeClient::DEVICE_KEY;
  async_caller_->AsyncTpmAttestationFinishCertRequest(data,
                                                      key_type,
                                                      name,
                                                      base::Bind(callback));
}

}  // namespace attestation
}  // namespace chromeos
