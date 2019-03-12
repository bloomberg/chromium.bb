// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains structures to implement the CTAP2 PIN protocol, version
// one. See
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorClientPIN

#ifndef DEVICE_FIDO_PIN_H_
#define DEVICE_FIDO_PIN_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "components/cbor/values.h"

namespace device {
namespace pin {

// kProtocolVersion is the version of the PIN protocol that this code
// implements.
constexpr int kProtocolVersion = 1;

// IsValid returns true if |pin|, which must be UTF-8, is a syntactically valid
// PIN.
COMPONENT_EXPORT(DEVICE_FIDO) bool IsValid(const std::string& pin);

// kMinBytes is the minimum number of *bytes* of PIN data that a CTAP2 device
// will accept. Since the PIN is UTF-8 encoded, this could be a single code
// point. However, the platform is supposed to additionally enforce a 4
// *character* minimum
constexpr size_t kMinBytes = 4;
// kMaxBytes is the maximum number of bytes of PIN data that a CTAP2 device will
// accept.
constexpr size_t kMaxBytes = 63;

// RetriesRequest asks an authenticator for the number of remaining PIN attempts
// before the device is locked.
struct RetriesRequest {
  std::vector<uint8_t> EncodeAsCBOR() const;
};

// RetriesResponse reflects an authenticator's response to a |RetriesRequest|.
struct RetriesResponse {
  static base::Optional<RetriesResponse> Parse(
      base::span<const uint8_t> buffer);

  // retries is the number of PIN attempts remaining before the authenticator
  // locks.
  int retries;

 private:
  RetriesResponse();
};

// KeyAgreementRequest asks an authenticator for an ephemeral ECDH key for
// encrypting PIN material in future requests.
struct KeyAgreementRequest {
  std::vector<uint8_t> EncodeAsCBOR() const;
};

// KeyAgreementResponse reflects an authenticator's response to a
// |KeyAgreementRequest| and is also used as representation of the
// authenticator's ephemeral key.
struct KeyAgreementResponse {
  static base::Optional<KeyAgreementResponse> Parse(
      base::span<const uint8_t> buffer);
  static base::Optional<KeyAgreementResponse> ParseFromCOSE(
      const cbor::Value::MapValue& cose_key);

  // x and y contain the big-endian coordinates of a P-256 point. It is ensured
  // that this is a valid point on the curve.
  uint8_t x[32], y[32];

 private:
  KeyAgreementResponse();
};

// SetRequest sets an initial PIN on an authenticator. (This is distinct from
// changing a PIN.)
class SetRequest {
 public:
  // IsValid(pin) must be true.
  SetRequest(const std::string& pin, const KeyAgreementResponse& peer_key);

  std::vector<uint8_t> EncodeAsCBOR() const;

 private:
  const KeyAgreementResponse peer_key_;
  uint8_t pin_[kMaxBytes + 1];
};

struct EmptyResponse {
  static base::Optional<EmptyResponse> Parse(base::span<const uint8_t> buffer);
};

// ChangeRequest changes the PIN on an authenticator that already has a PIN set.
// (This is distinct from setting an initial PIN.)
class ChangeRequest {
 public:
  // IsValid(new_pin) must be true.
  ChangeRequest(const std::string& old_pin,
                const std::string& new_pin,
                const KeyAgreementResponse& peer_key);

  std::vector<uint8_t> EncodeAsCBOR() const;

 private:
  const KeyAgreementResponse peer_key_;
  uint8_t old_pin_hash_[16];
  uint8_t new_pin_[kMaxBytes + 1];
};

// ResetRequest resets an authenticator, which should invalidate all
// credentials and clear any configured PIN. This is not strictly a
// PIN-related command, but is generally used to reset a PIN and so is
// included here.
struct ResetRequest {
  std::vector<uint8_t> EncodeAsCBOR() const;
};

using ResetResponse = EmptyResponse;

// TokenRequest requests a pin-token from an authenticator. These tokens can be
// used to show user-verification in other operations, e.g. when getting an
// assertion.
class TokenRequest {
 public:
  TokenRequest(const std::string& pin, const KeyAgreementResponse& peer_key);
  ~TokenRequest();
  TokenRequest(TokenRequest&&);
  TokenRequest(const TokenRequest&) = delete;

  // shared_key returns the shared ECDH key that was used to encrypt the PIN.
  // This is needed to decrypt the response.
  const std::array<uint8_t, 32>& shared_key() const;

  std::vector<uint8_t> EncodeAsCBOR() const;

 private:
  std::array<uint8_t, 32> shared_key_;
  cbor::Value::MapValue cose_key_;
  uint8_t pin_hash_[16];
};

// TokenResponse represents the response to a pin-token request. In order to
// decrypt a response, the shared key from the request is needed. Once a pin-
// token has been decrypted, it can be used to calculate the pinAuth parameters
// needed to show user-verification in future operations.
class TokenResponse {
 public:
  ~TokenResponse();
  TokenResponse(const TokenResponse&);

  static base::Optional<TokenResponse> Parse(std::array<uint8_t, 32> shared_key,
                                             base::span<const uint8_t> buffer);

  // PinAuth returns a pinAuth parameter for a request that will use the given
  // client-data hash.
  std::vector<uint8_t> PinAuth(const std::array<uint8_t, 32> client_data_hash);

 private:
  TokenResponse();

  std::vector<uint8_t> token_;
};

}  // namespace pin
}  // namespace device

#endif  // DEVICE_FIDO_PIN_H_
