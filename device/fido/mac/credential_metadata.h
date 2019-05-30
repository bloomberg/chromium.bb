// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides methods to encode credential metadata for storage in the
// macOS keychain.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/aead.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

namespace device {

class PublicKeyCredentialUserEntity;

namespace fido {
namespace mac {

// UserEntity loosely corresponds to a PublicKeyCredentialUserEntity
// (https://www.w3.org/TR/webauthn/#sctn-user-credential-params). Values of
// this type should be moved whenever possible.
struct COMPONENT_EXPORT(DEVICE_FIDO) UserEntity {
 public:
  static UserEntity FromPublicKeyCredentialUserEntity(
      const PublicKeyCredentialUserEntity&);

  UserEntity(std::vector<uint8_t> id_, std::string name_, std::string display_);
  UserEntity(const UserEntity&);
  UserEntity(UserEntity&&);
  UserEntity& operator=(UserEntity&&);
  ~UserEntity();

  PublicKeyCredentialUserEntity ToPublicKeyCredentialUserEntity();

  std::vector<uint8_t> id;
  std::string name;
  std::string display_name;
};

// Generates a random secret for encrypting and authenticating credential
// metadata for storage in the macOS keychain.
//
// Chrome stores this secret in the Profile Prefs. This allows us to logically
// separate credentials per Chrome profile despite being stored in the same
// keychain. It also guarantees that account metadata in the OS keychain is
// rendered unusable after the Chrome profile and the associated encryption key
// have been deleted, in order to limit leakage of credential metadata into the
// keychain.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string GenerateCredentialMetadataSecret();

// SealCredentialId encrypts the given UserEntity into a credential id.
//
// Credential IDs have following format:
//
//    | version  |    nonce   | AEAD(pt=CBOR(user_entity), |
//    | (1 byte) | (12 bytes) |      nonce=nonce,          |
//    |          |            |      ad=(version, rpID))   |
//
// with version as 0x00, a random 12-byte nonce, and using AES-256-GCM as the
// AEAD.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> SealCredentialId(const std::string& secret,
                                                      const std::string& rp_id,
                                                      const UserEntity& user);

// UnsealCredentialId attempts to decrypt a UserEntity from a credential id.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<UserEntity> UnsealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id);

// EncodeRpIdAndUserId encodes the concatenation of RP ID and user ID for
// storage in the macOS keychain.
//
// This encoding allows lookup of credentials for a given RP and user but
// without the credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> EncodeRpIdAndUserId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> user_id);

// EncodeRpId encodes the given RP ID for storage in the macOS keychain.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> EncodeRpId(const std::string& secret,
                                       const std::string& rp_id);

// DecodeRpId attempts to decode a given RP ID from the keychain.
//
// This can be used to test whether a set of credential metadata was created
// under the given secret without knowing the RP ID (which would be required to
// unseal a credential ID).
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> DecodeRpId(const std::string& secret,
                                       const std::string& ciphertext);

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
