// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_type_converters.h"

#include <algorithm>

#include "device/fido/fido_constants.h"

namespace mojo {

using ::webauth::mojom::PublicKeyCredentialUserEntityPtr;
using ::webauth::mojom::PublicKeyCredentialRpEntityPtr;
using ::webauth::mojom::AuthenticatorTransport;
using ::webauth::mojom::PublicKeyCredentialType;
using ::webauth::mojom::PublicKeyCredentialParametersPtr;
using ::webauth::mojom::PublicKeyCredentialDescriptorPtr;
using ::webauth::mojom::AuthenticatorSelectionCriteriaPtr;
using ::webauth::mojom::AuthenticatorAttachment;
using ::webauth::mojom::UserVerificationRequirement;

// static
::device::FidoTransportProtocol
TypeConverter<::device::FidoTransportProtocol, AuthenticatorTransport>::Convert(
    const AuthenticatorTransport& input) {
  switch (input) {
    case AuthenticatorTransport::USB:
      return ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
    case AuthenticatorTransport::NFC:
      return ::device::FidoTransportProtocol::kNearFieldCommunication;
    case AuthenticatorTransport::BLE:
      return ::device::FidoTransportProtocol::kBluetoothLowEnergy;
  }
  NOTREACHED();
  return ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
}

// static
::device::CredentialType
TypeConverter<::device::CredentialType, PublicKeyCredentialType>::Convert(
    const PublicKeyCredentialType& input) {
  switch (input) {
    case PublicKeyCredentialType::PUBLIC_KEY:
      return ::device::CredentialType::kPublicKey;
  }
  NOTREACHED();
  return ::device::CredentialType::kPublicKey;
}

// static
std::vector<::device::PublicKeyCredentialParams::CredentialInfo>
TypeConverter<std::vector<::device::PublicKeyCredentialParams::CredentialInfo>,
              std::vector<PublicKeyCredentialParametersPtr>>::
    Convert(const std::vector<PublicKeyCredentialParametersPtr>& input) {
  std::vector<::device::PublicKeyCredentialParams::CredentialInfo>
      credential_params;
  credential_params.reserve(input.size());

  for (const auto& credential : input) {
    credential_params.emplace_back(
        ::device::PublicKeyCredentialParams::CredentialInfo{
            ConvertTo<::device::CredentialType>(credential->type),
            credential->algorithm_identifier});
  }

  return credential_params;
}

// static
std::vector<::device::PublicKeyCredentialDescriptor>
TypeConverter<std::vector<::device::PublicKeyCredentialDescriptor>,
              std::vector<PublicKeyCredentialDescriptorPtr>>::
    Convert(const std::vector<PublicKeyCredentialDescriptorPtr>& input) {
  std::vector<::device::PublicKeyCredentialDescriptor> credential_descriptors;
  credential_descriptors.reserve(input.size());

  for (const auto& credential : input) {
    credential_descriptors.emplace_back(::device::PublicKeyCredentialDescriptor(
        device::to_string(
            ConvertTo<::device::CredentialType>(credential->type)),
        credential->id));
  }
  return credential_descriptors;
}

// static
::device::UserVerificationRequirement
TypeConverter<::device::UserVerificationRequirement,
              UserVerificationRequirement>::
    Convert(const ::webauth::mojom::UserVerificationRequirement& input) {
  switch (input) {
    case UserVerificationRequirement::PREFERRED:
      return ::device::UserVerificationRequirement::kPreferred;
    case UserVerificationRequirement::REQUIRED:
      return ::device::UserVerificationRequirement::kRequired;
    case UserVerificationRequirement::DISCOURAGED:
      return ::device::UserVerificationRequirement::kDiscouraged;
  }
  NOTREACHED();
  return ::device::UserVerificationRequirement::kPreferred;
}

// static
::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment TypeConverter<
    ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment,
    AuthenticatorAttachment>::Convert(const AuthenticatorAttachment& input) {
  switch (input) {
    case AuthenticatorAttachment::NO_PREFERENCE:
      return ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment::
          kAny;
    case AuthenticatorAttachment::PLATFORM:
      return ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment::
          kPlatform;
    case AuthenticatorAttachment::CROSS_PLATFORM:
      return ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment::
          kCrossPlatform;
  }
  NOTREACHED();
  return ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment::
      kAny;
}

// static
::device::AuthenticatorSelectionCriteria
TypeConverter<::device::AuthenticatorSelectionCriteria,
              AuthenticatorSelectionCriteriaPtr>::
    Convert(const AuthenticatorSelectionCriteriaPtr& input) {
  return device::AuthenticatorSelectionCriteria(
      ConvertTo<
          ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment>(
          input->authenticator_attachment),
      input->require_resident_key,
      ConvertTo<::device::UserVerificationRequirement>(
          input->user_verification));
}

// static
::device::PublicKeyCredentialRpEntity
TypeConverter<::device::PublicKeyCredentialRpEntity,
              PublicKeyCredentialRpEntityPtr>::
    Convert(const PublicKeyCredentialRpEntityPtr& input) {
  device::PublicKeyCredentialRpEntity rp_entity(input->id);
  rp_entity.SetRpName(input->name);
  if (input->icon)
    rp_entity.SetRpIconUrl(*input->icon);

  return rp_entity;
}

// static
::device::PublicKeyCredentialUserEntity
TypeConverter<::device::PublicKeyCredentialUserEntity,
              PublicKeyCredentialUserEntityPtr>::
    Convert(const PublicKeyCredentialUserEntityPtr& input) {
  device::PublicKeyCredentialUserEntity user_entity(input->id);
  user_entity.SetUserName(input->name).SetDisplayName(input->display_name);
  if (input->icon)
    user_entity.SetIconUrl(*input->icon);

  return user_entity;
}

}  // namespace mojo
