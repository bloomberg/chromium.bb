// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/sha2.h"
#include "device/fido/fido_constants.h"
#include "device/fido/u2f_parsing_utils.h"
#include "net/cert/x509_util.h"

namespace device {


namespace {

// First byte of registration response is 0x05 for historical reasons
// not detailed in the spec.
constexpr uint8_t kU2fRegistrationResponseHeader = 0x05;

// The example attestation private key from the U2F spec at
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-example
//
// PKCS.8 encoded without encryption.
constexpr uint8_t kAttestationKey[]{
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
    0xf3, 0xfc, 0xcc, 0x0d, 0x00, 0xd8, 0x03, 0x19, 0x54, 0xf9, 0x08, 0x64,
    0xd4, 0x3c, 0x24, 0x7f, 0x4b, 0xf5, 0xf0, 0x66, 0x5c, 0x6b, 0x50, 0xcc,
    0x17, 0x74, 0x9a, 0x27, 0xd1, 0xcf, 0x76, 0x64, 0xa1, 0x44, 0x03, 0x42,
    0x00, 0x04, 0x8d, 0x61, 0x7e, 0x65, 0xc9, 0x50, 0x8e, 0x64, 0xbc, 0xc5,
    0x67, 0x3a, 0xc8, 0x2a, 0x67, 0x99, 0xda, 0x3c, 0x14, 0x46, 0x68, 0x2c,
    0x25, 0x8c, 0x46, 0x3f, 0xff, 0xdf, 0x58, 0xdf, 0xd2, 0xfa, 0x3e, 0x6c,
    0x37, 0x8b, 0x53, 0xd7, 0x95, 0xc4, 0xa4, 0xdf, 0xfb, 0x41, 0x99, 0xed,
    0xd7, 0x86, 0x2f, 0x23, 0xab, 0xaf, 0x02, 0x03, 0xb4, 0xb8, 0x91, 0x1b,
    0xa0, 0x56, 0x99, 0x94, 0xe1, 0x01};

std::vector<uint8_t> GetAttestationKey() {
  return std::vector<uint8_t>(std::begin(kAttestationKey),
                              std::end(kAttestationKey));
}

// Small helper to make the code below more readable.
template <class T>
void AppendTo(std::vector<uint8_t>* dst, const T& src) {
  dst->insert(dst->end(), std::begin(src), std::end(src));
}

// Returns an error response with the given status.
base::Optional<std::vector<uint8_t>> ErrorStatus(
    apdu::ApduResponse::Status status) {
  return apdu::ApduResponse(std::vector<uint8_t>(), status)
      .GetEncodedResponse();
}

}  // namespace

// VirtualFidoDevice::RegistrationData ----------------------------------------

VirtualFidoDevice::RegistrationData::RegistrationData() = default;
VirtualFidoDevice::RegistrationData::RegistrationData(
    std::unique_ptr<crypto::ECPrivateKey> private_key,
    std::vector<uint8_t> application_parameter,
    uint32_t counter)
    : private_key(std::move(private_key)),
      application_parameter(std::move(application_parameter)),
      counter(counter) {}
VirtualFidoDevice::RegistrationData::RegistrationData(RegistrationData&& data) =
    default;
VirtualFidoDevice::RegistrationData::~RegistrationData() = default;

VirtualFidoDevice::RegistrationData& VirtualFidoDevice::RegistrationData::
operator=(RegistrationData&& other) = default;

// VirtualFidoDevice::State ---------------------------------------------------

VirtualFidoDevice::State::State()
    : attestation_cert_common_name("Batch Certificate"),
      individual_attestation_cert_common_name("Individual Certificate") {}
VirtualFidoDevice::State::~State() = default;

bool VirtualFidoDevice::State::InjectRegistration(
    const std::vector<uint8_t>& credential_id,
    const std::string& relying_party_id) {
  std::vector<uint8_t> application_parameter(crypto::kSHA256Length);
  crypto::SHA256HashString(relying_party_id, application_parameter.data(),
                           application_parameter.size());

  auto private_key = crypto::ECPrivateKey::Create();
  if (!private_key) {
    return false;
  }
  RegistrationData registration(std::move(private_key),
                                std::move(application_parameter),
                                0 /* signature counter */);

  bool was_inserted;
  std::tie(std::ignore, was_inserted) =
      registrations.emplace(credential_id, std::move(registration));
  return was_inserted;
}

VirtualFidoDevice::VirtualFidoDevice()
    : state_(new State), weak_factory_(this) {}

// VirtualFidoDevice ----------------------------------------------------------

VirtualFidoDevice::VirtualFidoDevice(scoped_refptr<State> state)
    : state_(std::move(state)), weak_factory_(this) {}

VirtualFidoDevice::~VirtualFidoDevice() = default;

void VirtualFidoDevice::TryWink(WinkCallback cb) {
  std::move(cb).Run();
}

std::string VirtualFidoDevice::GetId() const {
  // Use our heap address to get a unique-ish number. (0xffe1 is a prime).
  return "VirtualFidoDevice-" + std::to_string((size_t)this % 0xffe1);
}

void VirtualFidoDevice::DeviceTransact(std::vector<uint8_t> command,
                                       DeviceCallback cb) {
  // Note, here we are using the code-under-test in this fake.
  auto parsed_command = apdu::ApduCommand::CreateFromMessage(command);

  base::Optional<std::vector<uint8_t>> response;

  switch (parsed_command->ins()) {
    case base::strict_cast<uint8_t>(U2fApduInstruction::kVersion):
      break;
    case base::strict_cast<uint8_t>(U2fApduInstruction::kRegister):
      response = DoRegister(parsed_command->ins(), parsed_command->p1(),
                            parsed_command->p2(), parsed_command->data());
      break;
    case base::strict_cast<uint8_t>(U2fApduInstruction::kSign):
      response = DoSign(parsed_command->ins(), parsed_command->p1(),
                        parsed_command->p2(), parsed_command->data());
      break;
    default:
      response = ErrorStatus(apdu::ApduResponse::Status::SW_INS_NOT_SUPPORTED);
  }

  // Call |callback| via the |MessageLoop| because |AuthenticatorImpl| doesn't
  // support callback hairpinning.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), std::move(response)));
}

base::WeakPtr<FidoDevice> VirtualFidoDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::Optional<std::vector<uint8_t>> VirtualFidoDevice::DoRegister(
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    base::span<const uint8_t> data) {
  if (data.size() != 64) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_LENGTH);
  }

  const bool individual_attestation_requested = p1 & kP1IndividualAttestation;
  // The spec says that the other bits of P1 should be zero. However, Chrome
  // sends Test User Presence (0x03) so we ignore those bits.

  auto challenge_param = data.first(32);
  auto application_parameter = data.last(32);

  // Create key to register.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  auto private_key = crypto::ECPrivateKey::Create();
  std::string public_key;
  bool status = private_key->ExportRawPublicKey(&public_key);
  DCHECK(status);
  public_key.insert(0, 1, 0x04);
  DCHECK_EQ(public_key.size(), 65ul);

  // Our key handles are simple hashes of the public key.
  std::vector<uint8_t> key_handle(32);
  crypto::SHA256HashString(public_key, key_handle.data(), key_handle.size());

  // Data to be signed.
  std::vector<uint8_t> sign_buffer;
  sign_buffer.reserve(1 + application_parameter.size() +
                      challenge_param.size() + key_handle.size() +
                      public_key.size());
  sign_buffer.push_back(0x00);
  AppendTo(&sign_buffer, application_parameter);
  AppendTo(&sign_buffer, challenge_param);
  AppendTo(&sign_buffer, key_handle);
  AppendTo(&sign_buffer, public_key);

  // Sign with attestation key.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::vector<uint8_t> sig;
  std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
  auto signer =
      crypto::ECSignatureCreator::Create(attestation_private_key.get());
  status = signer->Sign(sign_buffer.data(), sign_buffer.size(), &sig);
  DCHECK(status);

  constexpr uint32_t kAttestationCertSerialNumber = 1;
  std::string attestation_cert;
  if (!net::x509_util::CreateSelfSignedCert(
          attestation_private_key->key(), net::x509_util::DIGEST_SHA256,
          "CN=" + (individual_attestation_requested
                       ? state_->individual_attestation_cert_common_name
                       : state_->attestation_cert_common_name),
          kAttestationCertSerialNumber, base::Time::FromTimeT(1500000000),
          base::Time::FromTimeT(1500000000), &attestation_cert)) {
    DVLOG(2) << "Failed to create attestation certificate";
    return ErrorStatus(apdu::ApduResponse::Status::SW_INS_NOT_SUPPORTED);
  }

  // U2F response data.
  std::vector<uint8_t> response;
  response.reserve(1 + public_key.size() + 1 + key_handle.size() +
                   attestation_cert.size() + sig.size());
  response.push_back(kU2fRegistrationResponseHeader);
  AppendTo(&response, public_key);
  response.push_back(key_handle.size());
  AppendTo(&response, key_handle);
  AppendTo(&response, attestation_cert);
  AppendTo(&response, sig);

  // Store the registration. Because the key handle is the hashed public key we
  // just generated, no way this should already be registered.
  bool did_insert = false;
  std::tie(std::ignore, did_insert) = state_->registrations.emplace(
      std::move(key_handle),
      RegistrationData(std::move(private_key),
                       u2f_parsing_utils::Materialize(application_parameter),
                       1));
  DCHECK(did_insert);

  return apdu::ApduResponse(std::move(response),
                            apdu::ApduResponse::Status::SW_NO_ERROR)
      .GetEncodedResponse();
}

base::Optional<std::vector<uint8_t>> VirtualFidoDevice::DoSign(
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    base::span<const uint8_t> data) {
  if (!(p1 == kP1CheckOnly || p1 == kP1TupRequiredConsumed ||
        p1 == kP1IndividualAttestation) ||
      p2 != 0) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_DATA);
  }

  auto challenge_param = data.first(32);
  auto application_parameter = data.subspan(32, 32);
  size_t key_handle_length = data[64];
  if (data.size() != 32 + 32 + 1 + key_handle_length) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_LENGTH);
  }
  auto key_handle = data.last(key_handle_length);

  // Check if this is our key_handle and it's for this appId.
  auto it = state_->registrations.find(
      std::vector<uint8_t>(key_handle.cbegin(), key_handle.cend()));

  if (it == state_->registrations.end()) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_DATA);
  }

  base::span<const uint8_t> registered_app_id_hash =
      base::make_span(it->second.application_parameter);
  if (application_parameter != registered_app_id_hash) {
    // It's important this error looks identical to the previous one, as
    // tokens should not reveal the existence of keyHandles to unrelated appIds.
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_DATA);
  }

  auto& registration = it->second;
  ++registration.counter;

  // First create the part of the response that gets signed over.
  std::vector<uint8_t> response;
  response.push_back(0x01);  // Always pretend we got a touch.
  response.push_back(registration.counter >> 24);
  response.push_back(registration.counter >> 16);
  response.push_back(registration.counter >> 8);
  response.push_back(registration.counter);

  std::vector<uint8_t> sign_buffer;
  sign_buffer.reserve(application_parameter.size() + response.size() +
                      challenge_param.size());
  AppendTo(&sign_buffer, application_parameter);
  AppendTo(&sign_buffer, response);
  AppendTo(&sign_buffer, challenge_param);

  // Sign with credential key.
  std::vector<uint8_t> sig;
  auto signer =
      crypto::ECSignatureCreator::Create(registration.private_key.get());
  bool status = signer->Sign(sign_buffer.data(), sign_buffer.size(), &sig);
  DCHECK(status);

  // Add signature for full response.
  AppendTo(&response, sig);

  return apdu::ApduResponse(std::move(response),
                            apdu::ApduResponse::Status::SW_NO_ERROR)
      .GetEncodedResponse();
}

}  // namespace device
