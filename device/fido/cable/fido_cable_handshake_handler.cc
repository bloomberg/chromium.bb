// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/noise.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

// Length of CBOR encoded authenticator hello message concatenated with
// 16 byte message authentication code.
constexpr size_t kCableAuthenticatorHandshakeMessageSize = 66;

// Length of CBOR encoded client hello message concatenated with 16 byte message
// authenticator code.
constexpr size_t kClientHelloMessageSize = 58;

constexpr size_t kCableHandshakeMacMessageSize = 16;

base::Optional<std::array<uint8_t, kClientHelloMessageSize>>
ConstructHandshakeMessage(base::StringPiece handshake_key,
                          base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto client_hello = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(client_hello);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return base::nullopt;

  std::array<uint8_t, 32> client_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringPiece(*client_hello),
                 client_hello_mac.data(), client_hello_mac.size())) {
    return base::nullopt;
  }

  DCHECK_EQ(kClientHelloMessageSize,
            client_hello->size() + kCableHandshakeMacMessageSize);
  std::array<uint8_t, kClientHelloMessageSize> handshake_message;
  std::copy(client_hello->begin(), client_hello->end(),
            handshake_message.begin());
  std::copy(client_hello_mac.begin(),
            client_hello_mac.begin() + kCableHandshakeMacMessageSize,
            handshake_message.begin() + client_hello->size());

  return handshake_message;
}

}  // namespace

FidoCableHandshakeHandler::~FidoCableHandshakeHandler() {}

FidoCableV1HandshakeHandler::FidoCableV1HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      nonce_(fido_parsing_utils::Materialize(nonce)),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)),
      handshake_key_(crypto::HkdfSha256(
          fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
          fido_parsing_utils::ConvertToStringPiece(nonce_),
          kCableHandshakeKeyInfo,
          /*derived_key_size=*/32)) {
  crypto::RandBytes(client_session_random_.data(),
                    client_session_random_.size());
}

FidoCableV1HandshakeHandler::~FidoCableV1HandshakeHandler() = default;

void FidoCableV1HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);
  if (!handshake_message) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  FIDO_LOG(DEBUG) << "Sending the caBLE handshake message";
  cable_device_->SendHandshakeMessage(
      fido_parsing_utils::Materialize(*handshake_message), std::move(callback));
}

bool FidoCableV1HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key_))
    return false;

  if (response.size() != kCableAuthenticatorHandshakeMessageSize) {
    return false;
  }

  const auto authenticator_hello = response.first(
      kCableAuthenticatorHandshakeMessageSize - kCableHandshakeMacMessageSize);
  if (!hmac.VerifyTruncated(
          fido_parsing_utils::ConvertToStringPiece(authenticator_hello),
          fido_parsing_utils::ConvertToStringPiece(
              response.subspan(authenticator_hello.size())))) {
    return false;
  }

  const auto authenticator_hello_cbor = cbor::Reader::Read(authenticator_hello);
  if (!authenticator_hello_cbor || !authenticator_hello_cbor->is_map() ||
      authenticator_hello_cbor->GetMap().size() != 2) {
    return false;
  }

  const auto authenticator_hello_msg =
      authenticator_hello_cbor->GetMap().find(cbor::Value(0));
  if (authenticator_hello_msg == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_hello_msg->second.is_string() ||
      authenticator_hello_msg->second.GetString() !=
          kCableAuthenticatorHelloMessage) {
    return false;
  }

  const auto authenticator_random_nonce =
      authenticator_hello_cbor->GetMap().find(cbor::Value(1));
  if (authenticator_random_nonce == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_random_nonce->second.is_bytestring() ||
      authenticator_random_nonce->second.GetBytestring().size() != 16) {
    return false;
  }

  cable_device_->SetV1EncryptionData(
      base::make_span<32>(
          GetEncryptionKeyAfterSuccessfulHandshake(base::make_span<16>(
              authenticator_random_nonce->second.GetBytestring()))),
      nonce_);

  return true;
}

std::vector<uint8_t>
FidoCableV1HandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return crypto::HkdfSha256(session_pre_key_, crypto::SHA256Hash(nonce_message),
                            kCableDeviceEncryptionKeyInfo,
                            /*derived_key_length=*/32);
}

// kP256PointSize is the number of bytes in an X9.62 encoding of a P-256 point.
static constexpr size_t kP256PointSize = 65;

FidoCableV2HandshakeHandler::FidoCableV2HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 32> psk_gen_key,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    base::Optional<base::span<const uint8_t, kP256PointSize>> peer_identity,
    base::Optional<base::span<const uint8_t, kCableIdentityKeySeedSize>>
        local_seed,
    base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>
        pairing_callback)
    : cable_device_(cable_device),
      pairing_callback_(std::move(pairing_callback)),
      handshake_(psk_gen_key, nonce, eid, peer_identity, local_seed) {}

FidoCableV2HandshakeHandler::~FidoCableV2HandshakeHandler() = default;

void FidoCableV2HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  std::vector<uint8_t> message = handshake_.BuildInitialMessage();
  cable_device_->SendHandshakeMessage(std::move(message), std::move(callback));
}

bool FidoCableV2HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  base::Optional<std::pair<std::unique_ptr<cablev2::Crypter>,
                           base::Optional<std::unique_ptr<CableDiscoveryData>>>>
      result = handshake_.ProcessResponse(response);
  if (!result) {
    return false;
  }

  if (result->second.has_value()) {
    pairing_callback_.Run(std::move(result->second.value()));
  }

  cable_device_->SetV2EncryptionData(std::move(result->first));
  return true;
}

}  // namespace device
