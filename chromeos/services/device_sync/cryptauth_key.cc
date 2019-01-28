// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key.h"

#include "base/base64.h"
#include "crypto/sha2.h"

namespace chromeos {

namespace device_sync {

namespace {

// Strings used as the DictionaryValue keys in As*DictionaryValue().
const char kHandleDictKey[] = "handle";
const char kStatusDictKey[] = "status";
const char kTypeDictKey[] = "type";
const char kSymmetricKeyDictKey[] = "symmetric_key";
const char kPublicKeyDictKey[] = "public_key";
const char kPrivateKeyDictKey[] = "private_key";

// Returns the base64-encoded SHA256 hash of the input string.
std::string CreateHandle(const std::string& string_to_hash) {
  std::string handle;
  base::Base64Encode(crypto::SHA256HashString(string_to_hash), &handle);
  return handle;
}

bool IsSymmetricKeyType(cryptauthv2::KeyType type) {
  return (type == cryptauthv2::KeyType::RAW128 ||
          type == cryptauthv2::KeyType::RAW256);
}

bool IsAsymmetricKeyType(cryptauthv2::KeyType type) {
  return (type == cryptauthv2::KeyType::P256 ||
          type == cryptauthv2::KeyType::CURVE25519);
}

}  // namespace

// static
base::Optional<CryptAuthKey> CryptAuthKey::FromDictionary(
    const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  base::Optional<int> opt_status = dict.FindIntKey(kStatusDictKey);
  if (!opt_status)
    return base::nullopt;
  CryptAuthKey::Status status = static_cast<CryptAuthKey::Status>(*opt_status);

  base::Optional<int> opt_type = dict.FindIntKey(kTypeDictKey);
  if (!opt_type || !cryptauthv2::KeyType_IsValid(*opt_type))
    return base::nullopt;
  cryptauthv2::KeyType type = static_cast<cryptauthv2::KeyType>(*opt_type);

  const std::string* handle = dict.FindStringKey(kHandleDictKey);
  if (!handle || handle->empty())
    return base::nullopt;

  if (IsSymmetricKeyType(type)) {
    const std::string* encoded_symmetric_key =
        dict.FindStringKey(kSymmetricKeyDictKey);
    if (!encoded_symmetric_key || encoded_symmetric_key->empty())
      return base::nullopt;

    std::string decoded_symmetric_key;
    if (!base::Base64Decode(*encoded_symmetric_key, &decoded_symmetric_key))
      return base::nullopt;

    return CryptAuthKey(decoded_symmetric_key, status, type, *handle);
  }

  DCHECK(IsAsymmetricKeyType(type));
  const std::string* encoded_public_key = dict.FindStringKey(kPublicKeyDictKey);
  const std::string* encoded_private_key =
      dict.FindStringKey(kPrivateKeyDictKey);
  if (!encoded_public_key || encoded_public_key->empty() ||
      !encoded_private_key) {
    return base::nullopt;
  }

  std::string decoded_public_key, decoded_private_key;
  if (!base::Base64Decode(*encoded_public_key, &decoded_public_key) ||
      !base::Base64Decode(*encoded_private_key, &decoded_private_key)) {
    return base::nullopt;
  }

  return CryptAuthKey(decoded_public_key, decoded_private_key, status, type,
                      *handle);
}

CryptAuthKey::CryptAuthKey(const std::string& symmetric_key,
                           Status status,
                           cryptauthv2::KeyType type,
                           const base::Optional<std::string>& handle)
    : symmetric_key_(symmetric_key),
      status_(status),
      type_(type),
      handle_(handle ? *handle : CreateHandle(symmetric_key)) {
  DCHECK(IsSymmetricKey());
  DCHECK(!symmetric_key_.empty());
  DCHECK(!handle_.empty());
}

CryptAuthKey::CryptAuthKey(const std::string& public_key,
                           const std::string& private_key,
                           Status status,
                           cryptauthv2::KeyType type,
                           const base::Optional<std::string>& handle)
    : public_key_(public_key),
      private_key_(private_key),
      status_(status),
      type_(type),
      handle_(handle ? *handle : CreateHandle(public_key)) {
  DCHECK(IsAsymmetricKey());
  DCHECK(!public_key_.empty());
  DCHECK(!handle_.empty());
}

CryptAuthKey::CryptAuthKey(const CryptAuthKey&) = default;

CryptAuthKey::~CryptAuthKey() = default;

bool CryptAuthKey::IsSymmetricKey() const {
  return IsSymmetricKeyType(type_);
}

bool CryptAuthKey::IsAsymmetricKey() const {
  return IsAsymmetricKeyType(type_);
}

base::Value CryptAuthKey::AsSymmetricKeyDictionary() const {
  DCHECK(IsSymmetricKey());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kHandleDictKey, base::Value(handle_));
  dict.SetKey(kStatusDictKey, base::Value(status_));
  dict.SetKey(kTypeDictKey, base::Value(type_));

  std::string encoded_symmetric_key;
  base::Base64Encode(symmetric_key_, &encoded_symmetric_key);
  dict.SetKey(kSymmetricKeyDictKey, base::Value(encoded_symmetric_key));

  return dict;
}

base::Value CryptAuthKey::AsAsymmetricKeyDictionary() const {
  DCHECK(IsAsymmetricKey());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kHandleDictKey, base::Value(handle_));
  dict.SetKey(kStatusDictKey, base::Value(status_));
  dict.SetKey(kTypeDictKey, base::Value(type_));

  std::string encoded_public_key;
  base::Base64Encode(public_key_, &encoded_public_key);
  dict.SetKey(kPublicKeyDictKey, base::Value(encoded_public_key));
  std::string encoded_private_key;
  base::Base64Encode(private_key_, &encoded_private_key);
  dict.SetKey(kPrivateKeyDictKey, base::Value(encoded_private_key));

  return dict;
}

bool CryptAuthKey::operator==(const CryptAuthKey& other) const {
  return handle_ == other.handle_ && status_ == other.status_ &&
         type_ == other.type_ && symmetric_key_ == other.symmetric_key_ &&
         public_key_ == other.public_key_ && private_key_ == other.private_key_;
}

bool CryptAuthKey::operator!=(const CryptAuthKey& other) const {
  return !(*this == other);
}

}  // namespace device_sync

}  // namespace chromeos
