// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {
namespace pin {

// kProtocolVersion is the version of the PIN protocol that this code
// implements.
constexpr int kProtocolVersion = 1;

// Subcommand enumerates the subcommands to the main |authenticatorClientPIN|
// command. See
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorClientPIN
enum class Subcommand : uint8_t {
  kGetRetries = 0x01,
  kGetKeyAgreement = 0x02,
  kSetPIN = 0x03,
  kChangePIN = 0x04,
  kGetPINToken = 0x05,
};

// CommandKeys enumerates the keys in the top-level CBOR map for all PIN
// commands.
enum class CommandKeys : int {
  kProtocol = 1,
  kSubcommand = 2,
  kKeyAgreement = 3,
  kPINAuth = 4,
  kNewPINEnc = 5,
  kPINHashEnc = 6,
};

// HasAtLeastFourCodepoints returns true if |pin| is UTF-8 encoded and contains
// four or more code points. This reflects the "4 Unicode characters"
// requirement in CTAP2.
static bool HasAtLeastFourCodepoints(const std::string& pin) {
  base::i18n::UTF8CharIterator it(&pin);
  return it.Advance() && it.Advance() && it.Advance() && it.Advance();
}

bool IsValid(const std::string& pin) {
  return pin.size() >= kMinBytes && pin.size() <= kMaxBytes &&
         pin.back() != 0 && base::IsStringUTF8(pin) &&
         HasAtLeastFourCodepoints(pin);
}

// EncodePINCommand returns a serialised CTAP2 PIN command for the operation
// |subcommand|. Additional elements of the top-level CBOR map can be added with
// the optional |add_additional| callback.
static std::vector<uint8_t> EncodePINCommand(
    Subcommand subcommand,
    std::function<void(cbor::Value::MapValue*)> add_additional = nullptr) {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(CommandKeys::kProtocol), kProtocolVersion);
  map.emplace(static_cast<int>(CommandKeys::kSubcommand),
              static_cast<int>(subcommand));

  if (add_additional) {
    add_additional(&map);
  }

  auto serialized = cbor::Writer::Write(cbor::Value(map));
  serialized->insert(
      serialized->begin(),
      static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorClientPin));
  return *serialized;
}

std::vector<uint8_t> RetriesRequest::EncodeAsCBOR() const {
  return EncodePINCommand(Subcommand::kGetRetries);
}

RetriesResponse::RetriesResponse() = default;

// static
base::Optional<RetriesResponse> RetriesResponse::Parse(
    base::span<const uint8_t> buffer) {
  // The response status code (the first byte) has already been processed by
  // this point.
  if (buffer.empty()) {
    return base::nullopt;
  }

  base::Optional<cbor::Value> decoded_response =
      cbor::Reader::Read(buffer.subspan(1));
  if (!decoded_response || !decoded_response->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(cbor::Value(3));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }

  RetriesResponse ret;
  ret.retries = it->second.GetUnsigned();
  return ret;
}

std::vector<uint8_t> KeyAgreementRequest::EncodeAsCBOR() const {
  return EncodePINCommand(Subcommand::kGetKeyAgreement);
}

KeyAgreementResponse::KeyAgreementResponse() = default;

// PointFromKeyAgreementResponse returns an |EC_POINT| that represents the same
// P-256 point as |response|. It returns |nullopt| if |response| encodes an
// invalid point.
static base::Optional<bssl::UniquePtr<EC_POINT>> PointFromKeyAgreementResponse(
    const EC_GROUP* group,
    const KeyAgreementResponse& response) {
  bssl::UniquePtr<EC_POINT> ret(EC_POINT_new(group));

  BIGNUM x_bn, y_bn;
  BN_init(&x_bn);
  BN_init(&y_bn);
  BN_bin2bn(response.x, sizeof(response.x), &x_bn);
  BN_bin2bn(response.y, sizeof(response.y), &y_bn);
  const bool on_curve =
      EC_POINT_set_affine_coordinates_GFp(group, ret.get(), &x_bn, &y_bn,
                                          nullptr /* ctx */) == 1;
  BN_clear(&x_bn);
  BN_clear(&y_bn);

  if (!on_curve) {
    return base::nullopt;
  }

  return ret;
}

// static
base::Optional<KeyAgreementResponse> KeyAgreementResponse::Parse(
    base::span<const uint8_t> buffer) {
  // The response status code (the first byte) has already been processed by
  // this point.
  if (buffer.empty()) {
    return base::nullopt;
  }

  base::Optional<cbor::Value> decoded_response =
      cbor::Reader::Read(buffer.subspan(1));
  if (!decoded_response || !decoded_response->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = decoded_response->GetMap();

  // The ephemeral key is encoded as a COSE structure under key 1.
  auto it = response_map.find(cbor::Value(1));
  if (it == response_map.end() || !it->second.is_map()) {
    return base::nullopt;
  }
  const auto& cose_key = it->second.GetMap();

  // The COSE key must be a P-256 point. See
  // https://tools.ietf.org/html/rfc8152#section-7.1
  for (const auto& pair : std::vector<std::pair<int, int>>({
           {1 /* key type */, 2 /* elliptic curve, uncompressed */},
           {3 /* algorithm */, -25 /* ECDH, ephemeral–static, HKDF-SHA-256 */},
           {-1 /* curve */, 1 /* P-256 */},
       })) {
    auto it = cose_key.find(cbor::Value(pair.first));
    if (it == cose_key.end() || !it->second.is_integer() ||
        it->second.GetInteger() != pair.second) {
      return base::nullopt;
    }
  }

  // See https://tools.ietf.org/html/rfc8152#section-13.1.1
  const auto& x_it = cose_key.find(cbor::Value(-2));
  const auto& y_it = cose_key.find(cbor::Value(-3));
  if (x_it == cose_key.end() || y_it == cose_key.end() ||
      !x_it->second.is_bytestring() || !y_it->second.is_bytestring()) {
    return base::nullopt;
  }

  const auto& x = x_it->second.GetBytestring();
  const auto& y = y_it->second.GetBytestring();
  KeyAgreementResponse ret;
  if (x.size() != sizeof(ret.x) || y.size() != sizeof(ret.y)) {
    return base::nullopt;
  }
  memcpy(ret.x, x.data(), sizeof(ret.x));
  memcpy(ret.y, y.data(), sizeof(ret.y));

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

  // Check that the point is on the curve.
  auto point = PointFromKeyAgreementResponse(group.get(), ret);
  if (!point) {
    return base::nullopt;
  }

  return ret;
}

SetRequest::SetRequest(const std::string& pin,
                       const KeyAgreementResponse& peer_key)
    : peer_key_(peer_key) {
  DCHECK(IsValid(pin));
  memset(pin_, 0, sizeof(pin_));
  memcpy(pin_, pin.data(), pin.size());
}

// SHA256KDF implements CTAP2's KDF, which just runs SHA-256 on the x-coordinate
// of the result. The function signature is such that it fits into OpenSSL's
// ECDH API.
static void* SHA256KDF(const void* in,
                       size_t in_len,
                       void* out,
                       size_t* out_len) {
  DCHECK_GE(*out_len, static_cast<size_t>(SHA256_DIGEST_LENGTH));
  SHA256(reinterpret_cast<const uint8_t*>(in), in_len,
         reinterpret_cast<uint8_t*>(out));
  *out_len = SHA256_DIGEST_LENGTH;
  return out;
}

// CalculateSharedKey generates and returns an ephemeral key, and writes the
// shared key between that ephemeral key and the authenticator's ephemeral key
// (from |peers_key|) to |out_shared_key|.
static cbor::Value::MapValue CalculateSharedKey(
    const KeyAgreementResponse& peers_key,
    uint8_t out_shared_key[SHA256_DIGEST_LENGTH]) {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  auto peers_point =
      PointFromKeyAgreementResponse(EC_KEY_get0_group(key.get()), peers_key);
  CHECK_EQ(static_cast<int>(SHA256_DIGEST_LENGTH),
           ECDH_compute_key(out_shared_key, SHA256_DIGEST_LENGTH,
                            peers_point->get(), key.get(), SHA256KDF));

  // X9.62 is the standard for serialising elliptic-curve points.
  uint8_t x962[1 /* type byte */ + 32 /* x */ + 32 /* y */];
  CHECK_EQ(sizeof(x962),
           EC_POINT_point2oct(EC_KEY_get0_group(key.get()),
                              EC_KEY_get0_public_key(key.get()),
                              POINT_CONVERSION_UNCOMPRESSED, x962, sizeof(x962),
                              nullptr /* BN_CTX */));

  cbor::Value::MapValue cose_key;
  cose_key.emplace(1 /* key type */, 2 /* uncompressed elliptic curve */);
  cose_key.emplace(3 /* algorithm */,
                   2 /* ECDH, ephemeral–static, HKDF-SHA-256 */);
  cose_key.emplace(-1 /* curve */, 1 /* P-256 */);
  cose_key.emplace(-2 /* x */, base::span<const uint8_t>(x962 + 1, 32));
  cose_key.emplace(-3 /* y */, base::span<const uint8_t>(x962 + 33, 32));

  return cose_key;
}

std::vector<uint8_t> SetRequest::EncodeAsCBOR() const {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#settingNewPin
  uint8_t shared_key[SHA256_DIGEST_LENGTH];
  auto cose_key = CalculateSharedKey(peer_key_, shared_key);

  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};
  CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr, shared_key,
                           kZeroIV));

  static_assert((sizeof(pin_) % AES_BLOCK_SIZE) == 0,
                "pin_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(pin_) + AES_BLOCK_SIZE];
  CHECK(EVP_Cipher(&aes_ctx, encrypted_pin, pin_, sizeof(pin_)));
  EVP_CIPHER_CTX_cleanup(&aes_ctx);

  uint8_t pin_auth[SHA256_DIGEST_LENGTH];
  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), shared_key, sizeof(shared_key), encrypted_pin,
             sizeof(pin_), pin_auth, &hmac_bytes));
  DCHECK_EQ(sizeof(pin_auth), static_cast<size_t>(hmac_bytes));

  return EncodePINCommand(
      Subcommand::kSetPIN,
      [&cose_key, &encrypted_pin, &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(CommandKeys::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(static_cast<int>(CommandKeys::kNewPINEnc),
                     // Note that the final AES block of |encrypted_pin| is
                     // discarded because CTAP2 doesn't include any padding.
                     base::span<const uint8_t>(encrypted_pin, sizeof(pin_)));
        map->emplace(static_cast<int>(CommandKeys::kPINAuth),
                     base::span<const uint8_t>(pin_auth));
      });
}

ChangeRequest::ChangeRequest(const std::string& old_pin,
                             const std::string& new_pin,
                             const KeyAgreementResponse& peer_key)
    : peer_key_(peer_key) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(old_pin.data()), old_pin.size(),
         digest);
  memcpy(old_pin_hash_, digest, sizeof(old_pin_hash_));

  DCHECK(IsValid(new_pin));
  memset(new_pin_, 0, sizeof(new_pin_));
  memcpy(new_pin_, new_pin.data(), new_pin.size());
}

// static
base::Optional<EmptyResponse> EmptyResponse::Parse(
    base::span<const uint8_t> buffer) {
  // The response status code (the first byte) has already been processed by
  // this point.
  if (buffer.size() != 1) {
    return base::nullopt;
  }

  EmptyResponse ret;
  return ret;
}

std::vector<uint8_t> ChangeRequest::EncodeAsCBOR() const {
  // See
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#changingExistingPin
  uint8_t shared_key[SHA256_DIGEST_LENGTH];
  auto cose_key = CalculateSharedKey(peer_key_, shared_key);

  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};

  CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr, shared_key,
                           kZeroIV));
  static_assert((sizeof(new_pin_) % AES_BLOCK_SIZE) == 0,
                "new_pin_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(new_pin_) + AES_BLOCK_SIZE];
  CHECK(EVP_Cipher(&aes_ctx, encrypted_pin, new_pin_, sizeof(new_pin_)));

  CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr, shared_key,
                           kZeroIV));
  static_assert((sizeof(old_pin_hash_) % AES_BLOCK_SIZE) == 0,
                "old_pin_hash_ is not a multiple of the AES block size");
  uint8_t old_pin_hash_enc[sizeof(old_pin_hash_) + AES_BLOCK_SIZE];
  CHECK(EVP_Cipher(&aes_ctx, old_pin_hash_enc, old_pin_hash_,
                   sizeof(old_pin_hash_)));

  EVP_CIPHER_CTX_cleanup(&aes_ctx);

  uint8_t ciphertexts_concat[sizeof(new_pin_) + sizeof(old_pin_hash_)];
  // Note that the final AES blocks of |encrypted_pin| and |old_pin_hash_enc|
  // are discarded because CTAP2 doesn't include any padding.
  memcpy(ciphertexts_concat, encrypted_pin, sizeof(new_pin_));
  memcpy(ciphertexts_concat + sizeof(new_pin_), old_pin_hash_enc,
         sizeof(old_pin_hash_));

  uint8_t pin_auth[SHA256_DIGEST_LENGTH];
  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), shared_key, sizeof(shared_key), ciphertexts_concat,
             sizeof(ciphertexts_concat), pin_auth, &hmac_bytes));
  DCHECK_EQ(sizeof(pin_auth), static_cast<size_t>(hmac_bytes));

  return EncodePINCommand(
      Subcommand::kChangePIN, [&cose_key, &encrypted_pin, &old_pin_hash_enc,
                               &pin_auth](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(CommandKeys::kKeyAgreement),
                     std::move(cose_key));
        map->emplace(
            static_cast<int>(CommandKeys::kPINHashEnc),
            base::span<const uint8_t>(old_pin_hash_enc, sizeof(old_pin_hash_)));
        map->emplace(
            static_cast<int>(CommandKeys::kNewPINEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(new_pin_)));
        map->emplace(static_cast<int>(CommandKeys::kPINAuth),
                     base::span<const uint8_t>(pin_auth));
      });
}

std::vector<uint8_t> ResetRequest::EncodeAsCBOR() const {
  return {static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorReset)};
}

TokenRequest::TokenRequest(const std::string& pin,
                           const KeyAgreementResponse& peer_key)
    : cose_key_(CalculateSharedKey(peer_key, shared_key_.data())) {
  DCHECK_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), shared_key_.size());
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(pin.data()), pin.size(), digest);
  memcpy(pin_hash_, digest, sizeof(pin_hash_));
}

TokenRequest::~TokenRequest() = default;

const std::array<uint8_t, 32>& TokenRequest::shared_key() const {
  return shared_key_;
}

std::vector<uint8_t> TokenRequest::EncodeAsCBOR() const {
  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};

  CHECK(EVP_EncryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                           shared_key_.data(), kZeroIV));
  static_assert((sizeof(pin_hash_) % AES_BLOCK_SIZE) == 0,
                "pin_hash_ is not a multiple of the AES block size");
  uint8_t encrypted_pin[sizeof(pin_hash_) + AES_BLOCK_SIZE];
  CHECK(EVP_Cipher(&aes_ctx, encrypted_pin, pin_hash_, sizeof(pin_hash_)));
  EVP_CIPHER_CTX_cleanup(&aes_ctx);

  return EncodePINCommand(
      Subcommand::kGetPINToken,
      [this, &encrypted_pin](cbor::Value::MapValue* map) {
        map->emplace(static_cast<int>(CommandKeys::kKeyAgreement),
                     std::move(this->cose_key_));
        map->emplace(
            static_cast<int>(CommandKeys::kPINHashEnc),
            base::span<const uint8_t>(encrypted_pin, sizeof(pin_hash_)));
      });
}

TokenResponse::TokenResponse() = default;
TokenResponse::~TokenResponse() = default;
TokenResponse::TokenResponse(const TokenResponse&) = default;

base::Optional<TokenResponse> TokenResponse::Parse(
    std::array<uint8_t, 32> shared_key,
    base::span<const uint8_t> buffer) {
  // The response status code (the first byte) has already been processed by
  // this point.
  if (buffer.empty()) {
    return base::nullopt;
  }

  base::Optional<cbor::Value> decoded_response =
      cbor::Reader::Read(buffer.subspan(1));
  if (!decoded_response || !decoded_response->is_map()) {
    return base::nullopt;
  }
  const auto& response_map = decoded_response->GetMap();

  // The encrypted PIN-token is under key 2.
  auto it = response_map.find(cbor::Value(2));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  const auto& encrypted_token = it->second.GetBytestring();
  if (encrypted_token.size() % AES_BLOCK_SIZE != 0) {
    return base::nullopt;
  }

  EVP_CIPHER_CTX aes_ctx;
  EVP_CIPHER_CTX_init(&aes_ctx);
  const uint8_t kZeroIV[AES_BLOCK_SIZE] = {0};
  CHECK(EVP_DecryptInit_ex(&aes_ctx, EVP_aes_256_cbc(), nullptr,
                           shared_key.data(), kZeroIV));
  CHECK(EVP_CIPHER_CTX_set_padding(&aes_ctx, 0 /* no padding */));

  TokenResponse ret;
  ret.token_.resize(encrypted_token.size());
  CHECK(EVP_Cipher(&aes_ctx, ret.token_.data(), encrypted_token.data(),
                   encrypted_token.size()));
  EVP_CIPHER_CTX_cleanup(&aes_ctx);

  return ret;
}

std::vector<uint8_t> TokenResponse::PinAuth(
    const std::array<uint8_t, 32> client_data_hash) {
  std::vector<uint8_t> pin_auth;
  pin_auth.resize(SHA256_DIGEST_LENGTH);

  unsigned hmac_bytes;
  CHECK(HMAC(EVP_sha256(), token_.data(), token_.size(),
             client_data_hash.data(), client_data_hash.size(), pin_auth.data(),
             &hmac_bytes));
  DCHECK_EQ(pin_auth.size(), static_cast<size_t>(hmac_bytes));
  pin_auth.resize(16);
  return pin_auth;
}

}  // namespace pin
}  // namespace device
