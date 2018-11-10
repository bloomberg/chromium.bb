// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_ctap2_device.h"

#include <array>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "crypto/ec_private_key.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/ec_public_key.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kDeviceAaguid = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x01, 0x02, 0x03, 0x04,
     0x05, 0x06, 0x07, 0x08}};

std::vector<uint8_t> ConstructResponse(CtapDeviceResponseCode response_code,
                                       base::span<const uint8_t> data) {
  std::vector<uint8_t> response{base::strict_cast<uint8_t>(response_code)};
  fido_parsing_utils::Append(&response, data);
  return response;
}

void ReturnCtap2Response(
    FidoDevice::DeviceCallback cb,
    CtapDeviceResponseCode response_code,
    base::Optional<base::span<const uint8_t>> data = base::nullopt) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb),
                     ConstructResponse(response_code,
                                       data.value_or(std::vector<uint8_t>{}))));
}

bool AreMakeCredentialOptionsValid(const AuthenticatorSupportedOptions& options,
                                   const CtapMakeCredentialRequest& request) {
  if (request.resident_key_required() && !options.supports_resident_key())
    return false;

  return request.user_verification() !=
             UserVerificationRequirement::kRequired ||
         options.user_verification_availability() ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured;
}

bool AreGetAssertionOptionsValid(const AuthenticatorSupportedOptions& options,
                                 const CtapGetAssertionRequest& request) {
  if (request.user_presence_required() && !options.user_presence_required())
    return false;

  return request.user_verification() !=
             UserVerificationRequirement::kRequired ||
         options.user_verification_availability() ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured;
}

// Checks that whether the received MakeCredential request includes EA256
// algorithm in publicKeyCredParam.
bool AreMakeCredentialParamsValid(const CtapMakeCredentialRequest& request) {
  const auto& params =
      request.public_key_credential_params().public_key_credential_params();
  return std::any_of(
      params.begin(), params.end(), [](const auto& credential_info) {
        return credential_info.algorithm ==
               base::strict_cast<int>(CoseAlgorithmIdentifier::kCoseEs256);
      });
}

std::unique_ptr<ECPublicKey> ConstructECPublicKey(
    std::string public_key_string) {
  DCHECK_EQ(64u, public_key_string.size());

  const auto public_key_x_coordinate =
      base::as_bytes(base::make_span(public_key_string)).first(32);
  const auto public_key_y_coordinate =
      base::as_bytes(base::make_span(public_key_string)).last(32);
  return std::make_unique<ECPublicKey>(
      fido_parsing_utils::kEs256,
      fido_parsing_utils::Materialize(public_key_x_coordinate),
      fido_parsing_utils::Materialize(public_key_y_coordinate));
}

std::vector<uint8_t> ConstructSignatureBuffer(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash) {
  std::vector<uint8_t> signature_buffer;
  fido_parsing_utils::Append(&signature_buffer,
                             authenticator_data.SerializeToByteArray());
  fido_parsing_utils::Append(&signature_buffer, client_data_hash);
  return signature_buffer;
}

std::vector<uint8_t> ConstructMakeCredentialResponse(
    const base::Optional<std::vector<uint8_t>> attestation_certificate,
    base::span<const uint8_t> signature,
    AuthenticatorData authenticator_data) {
  cbor::Value::MapValue attestation_map;
  attestation_map.emplace("alg", -7);
  attestation_map.emplace("sig", fido_parsing_utils::Materialize(signature));

  if (attestation_certificate) {
    cbor::Value::ArrayValue certificate_chain;
    certificate_chain.emplace_back(std::move(*attestation_certificate));
    attestation_map.emplace("x5c", std::move(certificate_chain));
  }

  AuthenticatorMakeCredentialResponse make_credential_response(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      AttestationObject(
          std::move(authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              "packed", cbor::Value(std::move(attestation_map)))));
  return GetSerializedCtapDeviceResponse(make_credential_response);
}

std::vector<uint8_t> ConstructGetAssertionResponse(
    AuthenticatorData authenticator_data,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> key_handle) {
  AuthenticatorGetAssertionResponse response(
      std::move(authenticator_data),
      fido_parsing_utils::Materialize(signature));

  response.SetCredential({CredentialType::kPublicKey,
                          fido_parsing_utils::Materialize(key_handle)});
  response.SetNumCredentials(1);
  return GetSerializedCtapDeviceResponse(response);
}

bool IsMakeCredentialOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        if (!param.first.is_string())
          return false;

        const auto& key = param.first.GetString();
        return ((key == kResidentKeyMapKey || key == kUserVerificationMapKey) &&
                param.second.is_bool());
      });
}

bool AreMakeCredentialRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(request_map.begin(), request_map.end(),
                     [](const auto& param) {
                       if (!param.first.is_integer())
                         return false;

                       const auto& key = param.first.GetInteger();
                       return (key <= 9u && key >= 1u);
                     });
}

bool IsGetAssertionOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return std::all_of(
      option_map.begin(), option_map.end(), [](const auto& param) {
        if (!param.first.is_string())
          return false;

        const auto& key = param.first.GetString();
        return (key == kUserPresenceMapKey || key == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

bool AreGetAssertionRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return std::all_of(request_map.begin(), request_map.end(),
                     [](const auto& param) {
                       if (!param.first.is_integer())
                         return false;

                       const auto& key = param.first.GetInteger();
                       return (key <= 7u || key >= 1u);
                     });
}

}  // namespace

VirtualCtap2Device::VirtualCtap2Device()
    : VirtualFidoDevice(),
      device_info_(AuthenticatorGetInfoResponse({ProtocolVersion::kCtap},
                                                kDeviceAaguid)),
      weak_factory_(this) {}

VirtualCtap2Device::VirtualCtap2Device(scoped_refptr<State> state)
    : VirtualFidoDevice(std::move(state)),
      device_info_(AuthenticatorGetInfoResponse({ProtocolVersion::kCtap},
                                                kDeviceAaguid)),
      weak_factory_(this) {}

VirtualCtap2Device::~VirtualCtap2Device() = default;

// As all operations for VirtualCtap2Device are synchronous and we do not wait
// for user touch, Cancel command is no-op.
void VirtualCtap2Device::Cancel() {}

void VirtualCtap2Device::DeviceTransact(std::vector<uint8_t> command,
                                        DeviceCallback cb) {
  if (command.empty()) {
    ReturnCtap2Response(std::move(cb), CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }

  auto cmd_type = command[0];
  const auto request_bytes = base::make_span(command).subspan(1);
  CtapDeviceResponseCode response_code = CtapDeviceResponseCode::kCtap2ErrOther;
  std::vector<uint8_t> response_data;

  switch (static_cast<CtapRequestCommand>(cmd_type)) {
    case CtapRequestCommand::kAuthenticatorGetInfo:
      if (!request_bytes.empty()) {
        ReturnCtap2Response(std::move(cb),
                            CtapDeviceResponseCode::kCtap2ErrOther);
        return;
      }

      response_code = OnAuthenticatorGetInfo(&response_data);
      break;
    case CtapRequestCommand::kAuthenticatorMakeCredential:
      response_code = OnMakeCredential(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorGetAssertion:
      response_code = OnGetAssertion(request_bytes, &response_data);
      break;
    default:
      break;
  }

  // Call |callback| via the |MessageLoop| because |AuthenticatorImpl| doesn't
  // support callback hairpinning.
  ReturnCtap2Response(std::move(cb), response_code, std::move(response_data));
}

base::WeakPtr<FidoDevice> VirtualCtap2Device::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void VirtualCtap2Device::SetAuthenticatorSupportedOptions(
    AuthenticatorSupportedOptions options) {
  device_info_.SetOptions(std::move(options));
}

CtapDeviceResponseCode VirtualCtap2Device::OnMakeCredential(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  auto request_and_hash = ParseCtapMakeCredentialRequest(request_bytes);
  if (!request_and_hash) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapMakeCredentialRequest request = std::get<0>(*request_and_hash);
  CtapMakeCredentialRequest::ClientDataHash client_data_hash =
      std::get<1>(*request_and_hash);

  if (!AreMakeCredentialOptionsValid(device_info_.options(), request) ||
      !AreMakeCredentialParamsValid(request)) {
    DLOG(ERROR) << "Virtual CTAP2 device does not support options required by "
                   "the request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  // Client pin is not supported.
  if (request.pin_auth()) {
    DLOG(ERROR) << "Virtual CTAP2 device does not support client pin.";
    return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
  }

  // Check for already registered credentials.
  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(request.rp().rp_id());
  if (request.exclude_list()) {
    for (const auto& excluded_credential : *request.exclude_list()) {
      if (FindRegistrationData(excluded_credential.id(), rp_id_hash))
        return CtapDeviceResponseCode::kCtap2ErrCredentialExcluded;
    }
  }

  // Create key to register.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  auto private_key = crypto::ECPrivateKey::Create();
  std::string public_key;
  bool status = private_key->ExportRawPublicKey(&public_key);
  DCHECK(status);

  // Our key handles are simple hashes of the public key.
  auto hash = fido_parsing_utils::CreateSHA256Hash(public_key);
  std::vector<uint8_t> key_handle(hash.begin(), hash.end());
  std::array<uint8_t, 2> sha256_length = {0, crypto::kSHA256Length};

  std::array<uint8_t, 16> kZeroAaguid = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 0};
  base::span<const uint8_t, 16> aaguid(kDeviceAaguid);
  if (mutable_state()->self_attestation &&
      !mutable_state()->non_zero_aaguid_with_self_attestation) {
    aaguid = kZeroAaguid;
  }

  AttestedCredentialData attested_credential_data(
      aaguid, sha256_length, key_handle, ConstructECPublicKey(public_key));

  base::Optional<cbor::Value> extensions;
  if (request.hmac_secret()) {
    cbor::Value::MapValue extensions_map;
    extensions_map.emplace(cbor::Value(kExtensionHmacSecret),
                           cbor::Value(true));
    extensions = cbor::Value(std::move(extensions_map));
  }

  auto authenticator_data = ConstructAuthenticatorData(
      rp_id_hash, 01ul, std::move(attested_credential_data),
      std::move(extensions));
  auto sign_buffer =
      ConstructSignatureBuffer(authenticator_data, client_data_hash);

  // Sign with attestation key.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::vector<uint8_t> sig;
  std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
  status = Sign(attestation_private_key.get(), std::move(sign_buffer), &sig);
  DCHECK(status);

  base::Optional<std::vector<uint8_t>> attestation_cert;
  if (!mutable_state()->self_attestation) {
    attestation_cert = GenerateAttestationCertificate(
        false /* individual_attestation_requested */);
    if (!attestation_cert) {
      DLOG(ERROR) << "Failed to generate attestation certificate.";
      return CtapDeviceResponseCode::kCtap2ErrOther;
    }
  }

  *response = ConstructMakeCredentialResponse(std::move(attestation_cert), sig,
                                              std::move(authenticator_data));

  StoreNewKey(rp_id_hash, key_handle, std::move(private_key));
  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnGetAssertion(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  auto request_and_hash = ParseCtapGetAssertionRequest(request_bytes);
  if (!request_and_hash) {
    DLOG(ERROR) << "Incorrectly formatted GetAssertion request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapGetAssertionRequest request = std::get<0>(*request_and_hash);
  CtapGetAssertionRequest::ClientDataHash client_data_hash =
      std::get<1>(*request_and_hash);

  // Resident keys are not supported.
  if (!request.allow_list() || request.allow_list()->empty()) {
    DLOG(ERROR) << "Allowed credential list is empty, but Virtual CTAP2 device "
                   "does not support resident keys.";
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
  }

  // Client pin option is not supported.
  if (request.pin_auth()) {
    DLOG(ERROR) << "Virtual CTAP2 device does not support client pin.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  if (!AreGetAssertionOptionsValid(device_info_.options(), request)) {
    DLOG(ERROR) << "Unsupported options required from the request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request.rp_id());

  RegistrationData* found_data = nullptr;
  base::span<const uint8_t> credential_id;
  for (const auto& allowed_credential : *request.allow_list()) {
    if ((found_data =
             FindRegistrationData(allowed_credential.id(), rp_id_hash))) {
      credential_id = allowed_credential.id();
      break;
    }
  }

  if (!found_data)
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;

  found_data->counter++;
  auto authenticator_data = ConstructAuthenticatorData(
      rp_id_hash, found_data->counter, base::nullopt, base::nullopt);
  auto signature_buffer =
      ConstructSignatureBuffer(authenticator_data, client_data_hash);

  // Sign with the private key of the received key handle.
  std::vector<uint8_t> sig;
  auto* private_key = found_data->private_key.get();
  bool status = Sign(private_key, std::move(signature_buffer), &sig);
  DCHECK(status);

  *response = ConstructGetAssertionResponse(std::move(authenticator_data),
                                            std::move(sig), credential_id);
  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnAuthenticatorGetInfo(
    std::vector<uint8_t>* response) const {
  *response = EncodeToCBOR(device_info_);
  return CtapDeviceResponseCode::kSuccess;
}

AuthenticatorData VirtualCtap2Device::ConstructAuthenticatorData(
    base::span<const uint8_t, kRpIdHashLength> rp_id_hash,
    uint32_t current_signature_count,
    base::Optional<AttestedCredentialData> attested_credential_data,
    base::Optional<cbor::Value> extensions) {
  uint8_t flag =
      base::strict_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);
  std::array<uint8_t, kSignCounterLength> signature_counter;

  // Constructing AuthenticatorData for registration operation.
  if (attested_credential_data)
    flag |= base::strict_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  if (extensions) {
    flag |= base::strict_cast<uint8_t>(
        AuthenticatorData::Flag::kExtensionDataIncluded);
  }

  signature_counter[0] = (current_signature_count >> 24) & 0xff;
  signature_counter[1] = (current_signature_count >> 16) & 0xff;
  signature_counter[2] = (current_signature_count >> 8) & 0xff;
  signature_counter[3] = (current_signature_count)&0xff;

  return AuthenticatorData(rp_id_hash, flag, signature_counter,
                           std::move(attested_credential_data),
                           std::move(extensions));
}

base::Optional<std::pair<CtapMakeCredentialRequest,
                         CtapMakeCredentialRequest::ClientDataHash>>
ParseCtapMakeCredentialRequest(base::span<const uint8_t> request_bytes) {
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map())
    return base::nullopt;

  const auto& request_map = cbor_request->GetMap();
  if (!AreMakeCredentialRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(1));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring())
    return base::nullopt;

  const auto client_data_hash =
      base::make_span(client_data_hash_it->second.GetBytestring())
          .subspan<0, kClientDataHashLength>();

  const auto rp_entity_it = request_map.find(cbor::Value(2));
  if (rp_entity_it == request_map.end() || !rp_entity_it->second.is_map())
    return base::nullopt;

  auto rp_entity =
      PublicKeyCredentialRpEntity::CreateFromCBORValue(rp_entity_it->second);
  if (!rp_entity)
    return base::nullopt;

  const auto user_entity_it = request_map.find(cbor::Value(3));
  if (user_entity_it == request_map.end() || !user_entity_it->second.is_map())
    return base::nullopt;

  auto user_entity = PublicKeyCredentialUserEntity::CreateFromCBORValue(
      user_entity_it->second);
  if (!user_entity)
    return base::nullopt;

  const auto credential_params_it = request_map.find(cbor::Value(4));
  if (credential_params_it == request_map.end())
    return base::nullopt;

  auto credential_params = PublicKeyCredentialParams::CreateFromCBORValue(
      credential_params_it->second);
  if (!credential_params)
    return base::nullopt;

  CtapMakeCredentialRequest request(
      std::string() /* client_data_json */, std::move(*rp_entity),
      std::move(*user_entity), std::move(*credential_params));

  const auto exclude_list_it = request_map.find(cbor::Value(5));
  if (exclude_list_it != request_map.end()) {
    if (!exclude_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = exclude_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> exclude_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto excluded_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!excluded_credential)
        return base::nullopt;

      exclude_list.push_back(std::move(*excluded_credential));
    }
    request.SetExcludeList(std::move(exclude_list));
  }

  const auto extensions_it = request_map.find(cbor::Value(6));
  if (extensions_it != request_map.end()) {
    if (!extensions_it->second.is_map()) {
      return base::nullopt;
    }

    const auto& extensions = extensions_it->second.GetMap();
    const auto hmac_secret_it =
        extensions.find(cbor::Value(kExtensionHmacSecret));
    if (hmac_secret_it != extensions.end()) {
      if (!hmac_secret_it->second.is_bool()) {
        return base::nullopt;
      }
      request.SetHmacSecret(hmac_secret_it->second.GetBool());
    }
  }

  const auto option_it = request_map.find(cbor::Value(7));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsMakeCredentialOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto resident_key_option =
        option_map.find(cbor::Value(kResidentKeyMapKey));
    if (resident_key_option != option_map.end())
      request.SetResidentKeyRequired(resident_key_option->second.GetBool());

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end())
      request.SetUserVerification(
          uv_option->second.GetBool()
              ? UserVerificationRequirement::kRequired
              : UserVerificationRequirement::kDiscouraged);
  }

  const auto pin_auth_it = request_map.find(cbor::Value(8));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;
    request.SetPinAuth(pin_auth_it->second.GetBytestring());
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(9));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max())
      return base::nullopt;
    request.SetPinProtocol(pin_auth_it->second.GetUnsigned());
  }

  return std::make_pair(std::move(request),
                        fido_parsing_utils::Materialize(client_data_hash));
}

base::Optional<
    std::pair<CtapGetAssertionRequest, CtapGetAssertionRequest::ClientDataHash>>
ParseCtapGetAssertionRequest(base::span<const uint8_t> request_bytes) {
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map())
    return base::nullopt;

  const auto& request_map = cbor_request->GetMap();
  if (!AreGetAssertionRequestMapKeysCorrect(request_map))
    return base::nullopt;

  const auto rp_id_it = request_map.find(cbor::Value(1));
  if (rp_id_it == request_map.end() || !rp_id_it->second.is_string())
    return base::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(2));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring())
    return base::nullopt;

  const auto client_data_hash =
      base::make_span(client_data_hash_it->second.GetBytestring())
          .subspan<0, kClientDataHashLength>();

  CtapGetAssertionRequest request(rp_id_it->second.GetString(),
                                  std::string() /* client_data_json */);

  const auto allow_list_it = request_map.find(cbor::Value(3));
  if (allow_list_it != request_map.end()) {
    if (!allow_list_it->second.is_array())
      return base::nullopt;

    const auto& credential_descriptors = allow_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> allow_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto allowed_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!allowed_credential)
        return base::nullopt;

      allow_list.push_back(std::move(*allowed_credential));
    }
    request.SetAllowList(std::move(allow_list));
  }

  const auto option_it = request_map.find(cbor::Value(5));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return base::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsGetAssertionOptionMapFormatCorrect(option_map))
      return base::nullopt;

    const auto user_presence_option =
        option_map.find(cbor::Value(kUserPresenceMapKey));
    if (user_presence_option != option_map.end())
      request.SetUserPresenceRequired(user_presence_option->second.GetBool());

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end())
      request.SetUserVerification(
          uv_option->second.GetBool()
              ? UserVerificationRequirement::kRequired
              : UserVerificationRequirement::kPreferred);
  }

  const auto pin_auth_it = request_map.find(cbor::Value(6));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return base::nullopt;
    request.SetPinAuth(pin_auth_it->second.GetBytestring());
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(7));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max())
      return base::nullopt;
    request.SetPinProtocol(pin_auth_it->second.GetUnsigned());
  }

  return std::make_pair(std::move(request),
                        fido_parsing_utils::Materialize(client_data_hash));
}

}  // namespace device
