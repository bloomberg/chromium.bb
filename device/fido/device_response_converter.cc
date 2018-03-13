// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/device_response_converter.h"

#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/cbor/cbor_reader.h"
#include "components/cbor/cbor_writer.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_constants.h"

namespace device {

using CBOR = cbor::CBORValue;

CtapDeviceResponseCode GetResponseCode(const std::vector<uint8_t>& buffer) {
  if (buffer.empty())
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;

  auto code = static_cast<CtapDeviceResponseCode>(buffer[0]);
  return base::ContainsValue(GetCtapResponseCodeList(), code)
             ? code
             : CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
}

// Decodes byte array response from authenticator to CBOR value object and
// checks for correct encoding format. Then re-serialize the decoded CBOR value
// to byte array in format specified by the WebAuthN spec (i.e the keys for
// CBOR map value are converted from unsigned integers to string type.)
base::Optional<AuthenticatorMakeCredentialResponse>
ReadCTAPMakeCredentialResponse(CtapDeviceResponseCode response_code,
                               const std::vector<uint8_t>& buffer) {
  base::Optional<CBOR> decoded_response = cbor::CBORReader::Read(buffer);

  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  const auto& decoded_map = decoded_response->GetMap();
  CBOR::MapValue response_map;

  auto it = decoded_map.find(CBOR(1));
  if (it == decoded_map.end() || !it->second.is_string())
    return base::nullopt;

  response_map[CBOR("fmt")] = it->second.Clone();

  it = decoded_map.find(CBOR(2));
  if (it == decoded_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  response_map[CBOR("authData")] = it->second.Clone();

  it = decoded_map.find(CBOR(3));
  if (it == decoded_map.end() || !it->second.is_map())
    return base::nullopt;

  response_map[CBOR("attStmt")] = it->second.Clone();

  auto attestation_object =
      cbor::CBORWriter::Write(CBOR(std::move(response_map)));
  if (!attestation_object)
    return base::nullopt;

  return AuthenticatorMakeCredentialResponse(response_code,
                                             std::move(*attestation_object));
}

base::Optional<AuthenticatorGetAssertionResponse> ReadCTAPGetAssertionResponse(
    CtapDeviceResponseCode response_code,
    const std::vector<uint8_t>& buffer) {
  base::Optional<CBOR> decoded_response = cbor::CBORReader::Read(buffer);

  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(4));
  if (it == response_map.end() || !it->second.is_map())
    return base::nullopt;

  auto user = PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
  if (!user)
    return base::nullopt;

  it = response_map.find(CBOR(2));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;
  auto auth_data = it->second.GetBytestring();

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;
  auto signature = it->second.GetBytestring();

  AuthenticatorGetAssertionResponse response(
      response_code, std::move(auth_data), std::move(signature),
      std::move(*user));

  it = response_map.find(CBOR(1));
  if (it != response_map.end()) {
    auto descriptor =
        PublicKeyCredentialDescriptor::CreateFromCBORValue(it->second);
    if (!descriptor)
      return base::nullopt;

    response.SetCredential(std::move(*descriptor));
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.SetNumCredentials(it->second.GetUnsigned());
  }

  return base::Optional<AuthenticatorGetAssertionResponse>(std::move(response));
}

base::Optional<AuthenticatorGetInfoResponse> ReadCTAPGetInfoResponse(
    CtapDeviceResponseCode response_code,
    const std::vector<uint8_t>& buffer) {
  base::Optional<CBOR> decoded_response = cbor::CBORReader::Read(buffer);

  if (!decoded_response || !decoded_response->is_map())
    return base::nullopt;

  const auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(1));
  if (it == response_map.end() || !it->second.is_array())
    return base::nullopt;

  std::vector<std::string> versions;
  for (const auto& version : it->second.GetArray()) {
    if (!version.is_string())
      return base::nullopt;

    versions.push_back(version.GetString());
  }

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  AuthenticatorGetInfoResponse response(response_code, std::move(versions),
                                        it->second.GetBytestring());

  it = response_map.find(CBOR(2));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<std::string> extensions;
    for (const auto& extension : it->second.GetArray()) {
      if (!extension.is_string())
        return base::nullopt;

      extensions.push_back(extension.GetString());
    }
    response.SetExtensions(std::move(extensions));
  }

  it = response_map.find(CBOR(4));
  if (it != response_map.end()) {
    if (!it->second.is_map())
      return base::nullopt;

    const auto& option_map = it->second.GetMap();
    AuthenticatorSupportedOptions options;

    auto option_map_it = option_map.find(CBOR("plat"));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetIsPlatformDevice(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR("rk"));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetSupportsResidentKey(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR("up"));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetUserPresenceRequired(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR("uv"));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetUserVerificationRequired(option_map_it->second.GetBool());
    }

    option_map_it = option_map.find(CBOR("client_pin"));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.SetClientPinStored(option_map_it->second.GetBool());
    }
    response.SetOptions(std::move(options));
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.SetMaxMsgSize(it->second.GetUnsigned());
  }

  it = response_map.find(CBOR(6));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<uint8_t> supported_pin_protocols;
    for (const auto& protocol : it->second.GetArray()) {
      if (!protocol.is_unsigned())
        return base::nullopt;

      supported_pin_protocols.push_back(protocol.GetUnsigned());
    }
    response.SetPinProtocols(std::move(supported_pin_protocols));
  }

  return base::Optional<AuthenticatorGetInfoResponse>(std::move(response));
}

}  // namespace device
