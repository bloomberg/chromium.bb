// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_

#include <vector>

#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/platform/modules/webauthn/authenticator.mojom.h"

// TODO(hongjunchoi): Remove type converters and instead expose mojo interface
// directly from device/fido service.
// See: https://crbug.com/831209
namespace mojo {

template <>
struct TypeConverter<::device::FidoTransportProtocol,
                     ::blink::mojom::AuthenticatorTransport> {
  static ::device::FidoTransportProtocol Convert(
      const ::blink::mojom::AuthenticatorTransport& input);
};

template <>
struct TypeConverter<::blink::mojom::AuthenticatorTransport,
                     ::device::FidoTransportProtocol> {
  static ::blink::mojom::AuthenticatorTransport Convert(
      const ::device::FidoTransportProtocol& input);
};

template <>
struct TypeConverter<::device::CredentialType,
                     ::blink::mojom::PublicKeyCredentialType> {
  static ::device::CredentialType Convert(
      const ::blink::mojom::PublicKeyCredentialType& input);
};

template <>
struct TypeConverter<
    std::vector<::device::PublicKeyCredentialParams::CredentialInfo>,
    std::vector<::blink::mojom::PublicKeyCredentialParametersPtr>> {
  static std::vector<::device::PublicKeyCredentialParams::CredentialInfo>
  Convert(const std::vector<::blink::mojom::PublicKeyCredentialParametersPtr>&
              input);
};

template <>
struct TypeConverter<
    std::vector<::device::PublicKeyCredentialDescriptor>,
    std::vector<::blink::mojom::PublicKeyCredentialDescriptorPtr>> {
  static std::vector<::device::PublicKeyCredentialDescriptor> Convert(
      const std::vector<::blink::mojom::PublicKeyCredentialDescriptorPtr>&
          input);
};

template <>
struct TypeConverter<
    ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment,
    ::blink::mojom::AuthenticatorAttachment> {
  static ::device::AuthenticatorSelectionCriteria::AuthenticatorAttachment
  Convert(const ::blink::mojom::AuthenticatorAttachment& input);
};

template <>
struct TypeConverter<::device::UserVerificationRequirement,
                     ::blink::mojom::UserVerificationRequirement> {
  static ::device::UserVerificationRequirement Convert(
      const ::blink::mojom::UserVerificationRequirement& input);
};

template <>
struct TypeConverter<::device::AuthenticatorSelectionCriteria,
                     ::blink::mojom::AuthenticatorSelectionCriteriaPtr> {
  static ::device::AuthenticatorSelectionCriteria Convert(
      const ::blink::mojom::AuthenticatorSelectionCriteriaPtr& input);
};

template <>
struct TypeConverter<::device::PublicKeyCredentialRpEntity,
                     ::blink::mojom::PublicKeyCredentialRpEntityPtr> {
  static ::device::PublicKeyCredentialRpEntity Convert(
      const ::blink::mojom::PublicKeyCredentialRpEntityPtr& input);
};

template <>
struct TypeConverter<::device::PublicKeyCredentialUserEntity,
                     ::blink::mojom::PublicKeyCredentialUserEntityPtr> {
  static ::device::PublicKeyCredentialUserEntity Convert(
      const ::blink::mojom::PublicKeyCredentialUserEntityPtr& input);
};

template <>
struct TypeConverter<std::vector<::device::CableDiscoveryData>,
                     std::vector<::blink::mojom::CableAuthenticationPtr>> {
  static std::vector<::device::CableDiscoveryData> Convert(
      const std::vector<::blink::mojom::CableAuthenticationPtr>& input);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
