// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_device.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"

namespace device {

FidoDeviceAuthenticator::FidoDeviceAuthenticator(
    std::unique_ptr<FidoDevice> device)
    : device_(std::move(device)), weak_factory_(this) {}
FidoDeviceAuthenticator::~FidoDeviceAuthenticator() = default;

void FidoDeviceAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FidoDevice::DiscoverSupportedProtocolAndDeviceInfo,
          device()->GetWeakPtr(),
          base::BindOnce(&FidoDeviceAuthenticator::InitializeAuthenticatorDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void FidoDeviceAuthenticator::InitializeAuthenticatorDone(
    base::OnceClosure callback) {
  DCHECK(!options_);
  switch (device_->supported_protocol()) {
    case ProtocolVersion::kU2f:
      options_ = AuthenticatorSupportedOptions();
      break;
    case ProtocolVersion::kCtap:
      DCHECK(device_->device_info()) << "uninitialized device";
      options_ = device_->device_info()->options();
      break;
    case ProtocolVersion::kUnknown:
      NOTREACHED() << "uninitialized device";
      options_ = AuthenticatorSupportedOptions();
  }
  std::move(callback).Run();
}

void FidoDeviceAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                             MakeCredentialCallback callback) {
  DCHECK(!task_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());

  // Update the request to the "effective" user verification requirement.
  // https://w3c.github.io/webauthn/#effective-user-verification-requirement-for-credential-creation
  if (Options()->user_verification_availability ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    request.SetUserVerification(UserVerificationRequirement::kRequired);
  } else {
    request.SetUserVerification(UserVerificationRequirement::kDiscouraged);
  }

  // TODO(martinkr): Change FidoTasks to take all request parameters by const
  // reference, so we can avoid copying these from the RequestHandler.
  task_ = std::make_unique<MakeCredentialTask>(
      device_.get(), std::move(request), std::move(callback));
}

void FidoDeviceAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                           GetAssertionCallback callback) {
  DCHECK(!task_);
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());

  // Update the request to the "effective" user verification requirement.
  // https://w3c.github.io/webauthn/#effective-user-verification-requirement-for-assertion
  if (Options()->user_verification_availability ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    request.SetUserVerification(UserVerificationRequirement::kRequired);
  } else {
    request.SetUserVerification(UserVerificationRequirement::kDiscouraged);
  }

  task_ = std::make_unique<GetAssertionTask>(device_.get(), std::move(request),
                                             std::move(callback));
}

void FidoDeviceAuthenticator::GetTouch(base::OnceCallback<void()> callback) {
  // We want to flash and wait for a touch. With a U2F device, that can be
  // achieved by requesting a signature for a dummy credential ID, but CTAP2
  // devices will return an error immediately. Newer versions of the CTAP2 spec
  // include a provision for blocking for a touch when an empty pinAuth is
  // specified, but devices may exist that predate this part of the spec and
  // also the spec says that devices need only do that if they implement PIN
  // support. Therefore, in order to portably wait for a touch a dummy
  // credential is created. This does assume that the device supports ECDSA
  // P-256, however.
  if (device_->supported_protocol() == ProtocolVersion::kU2f) {
    CtapGetAssertionRequest req(".dummy", "");
    req.SetAllowList(
        {PublicKeyCredentialDescriptor(CredentialType::kPublicKey, {0})});
    GetAssertion(
        std::move(req),
        base::BindOnce(
            [](base::OnceCallback<void()> callback, CtapDeviceResponseCode,
               base::Optional<AuthenticatorGetAssertionResponse>) {
              std::move(callback).Run();
            },
            std::move(callback)));
  } else {
    PublicKeyCredentialUserEntity user({1} /* user ID */);
    // The user name is incorrectly marked as optional in the CTAP2 spec.
    user.SetUserName("dummy");
    CtapMakeCredentialRequest req(
        "" /* client_data_json */, PublicKeyCredentialRpEntity(".dummy"),
        std::move(user),
        PublicKeyCredentialParams(
            {{CredentialType::kPublicKey,
              base::strict_cast<int>(CoseAlgorithmIdentifier::kCoseEs256)}}));
    req.SetExcludeList({});
    // Set an empty pinAuth in case the device is a newer CTAP2 and understands
    // this to mean to block for touch.
    req.SetPinAuth({});

    MakeCredential(
        std::move(req),
        base::BindOnce(
            [](base::OnceCallback<void()> callback,
               CtapDeviceResponseCode status,
               base::Optional<AuthenticatorMakeCredentialResponse>) {
              // If the device didn't understand/process the request it may
              // fail immediately. Rather than count that as a touch, ignore
              // those cases completely.
              if (status == CtapDeviceResponseCode::kSuccess ||
                  status == CtapDeviceResponseCode::kCtap2ErrPinNotSet ||
                  status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
                std::move(callback).Run();
              }
            },
            std::move(callback)));
  }
}

void FidoDeviceAuthenticator::GetRetries(GetRetriesCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::RetriesRequest, pin::RetriesResponse>>(
      device_.get(), pin::RetriesRequest(), std::move(callback),
      base::BindOnce(&pin::RetriesResponse::Parse));
  operation_->Start();
}

void FidoDeviceAuthenticator::GetEphemeralKey(
    GetEphemeralKeyCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ =
      std::make_unique<Ctap2DeviceOperation<pin::KeyAgreementRequest,
                                            pin::KeyAgreementResponse>>(
          device_.get(), pin::KeyAgreementRequest(), std::move(callback),
          base::BindOnce(&pin::KeyAgreementResponse::Parse));
  operation_->Start();
}

void FidoDeviceAuthenticator::GetPINToken(
    std::string pin,
    const pin::KeyAgreementResponse& peer_key,
    GetPINTokenCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  pin::TokenRequest request(pin, peer_key);
  std::array<uint8_t, 32> shared_key = request.shared_key();
  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::TokenRequest, pin::TokenResponse>>(
      device_.get(), std::move(request), std::move(callback),
      base::BindOnce(&pin::TokenResponse::Parse, std::move(shared_key)));
  operation_->Start();
}

void FidoDeviceAuthenticator::SetPIN(const std::string& pin,
                                     pin::KeyAgreementResponse& peer_key,
                                     SetPINCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::SetRequest, pin::EmptyResponse>>(
      device_.get(), pin::SetRequest(pin, peer_key), std::move(callback),
      base::BindOnce(&pin::EmptyResponse::Parse));
  operation_->Start();
}

void FidoDeviceAuthenticator::ChangePIN(const std::string& old_pin,
                                        const std::string& new_pin,
                                        pin::KeyAgreementResponse& peer_key,
                                        SetPINCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::ChangeRequest, pin::EmptyResponse>>(
      device_.get(), pin::ChangeRequest(old_pin, new_pin, peer_key),
      std::move(callback), base::BindOnce(&pin::EmptyResponse::Parse));
  operation_->Start();
}

void FidoDeviceAuthenticator::Reset(ResetCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::ResetRequest, pin::ResetResponse>>(
      device_.get(), pin::ResetRequest(), std::move(callback),
      base::BindOnce(&pin::ResetResponse::Parse));
  operation_->Start();
}

void FidoDeviceAuthenticator::Cancel() {
  if (!task_)
    return;

  task_->CancelTask();
}

std::string FidoDeviceAuthenticator::GetId() const {
  return device_->GetId();
}

base::string16 FidoDeviceAuthenticator::GetDisplayName() const {
  return device_->GetDisplayName();
}

ProtocolVersion FidoDeviceAuthenticator::SupportedProtocol() const {
  DCHECK(device_->SupportedProtocolIsInitialized());
  return device_->supported_protocol();
}

const base::Optional<AuthenticatorSupportedOptions>&
FidoDeviceAuthenticator::Options() const {
  return options_;
}

base::Optional<FidoTransportProtocol>
FidoDeviceAuthenticator::AuthenticatorTransport() const {
  return device_->DeviceTransport();
}

bool FidoDeviceAuthenticator::IsInPairingMode() const {
  return device_->IsInPairingMode();
}

bool FidoDeviceAuthenticator::IsPaired() const {
  return device_->IsPaired();
}

#if defined(OS_WIN)
bool FidoDeviceAuthenticator::IsWinNativeApiAuthenticator() const {
  return false;
}
#endif  // defined(OS_WIN)

void FidoDeviceAuthenticator::SetTaskForTesting(
    std::unique_ptr<FidoTask> task) {
  task_ = std::move(task);
}

base::WeakPtr<FidoAuthenticator> FidoDeviceAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
