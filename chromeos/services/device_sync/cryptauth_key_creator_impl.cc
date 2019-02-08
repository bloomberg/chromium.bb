// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_creator_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"

namespace chromeos {

namespace device_sync {

namespace {

// The salt used in HKDF to derive symmetric keys from Diffie-Hellman handshake.
// This value is part of the CryptAuth v2 Enrollment specifications.
const char kSymmetricKeyDerivationSalt[] = "CryptAuth Enrollment";

bool IsValidSymmetricKeyType(const cryptauthv2::KeyType& type) {
  return type == cryptauthv2::KeyType::RAW128 ||
         type == cryptauthv2::KeyType::RAW256;
}

bool IsValidAsymmetricKeyType(const cryptauthv2::KeyType& type) {
  return type == cryptauthv2::KeyType::P256;
}

// If we need to create any symmetric keys, we first need to generate the
// client's ephemeral Diffie-Hellman key-pair and derive the handshake secret
// from the server's public key and the client's private key. This secret is
// used to derive new symmetric keys using HKDF.
bool IsClientEphemeralDhKeyNeeded(
    const base::flat_map<CryptAuthKeyBundle::Name,
                         CryptAuthKeyCreator::CreateKeyData>& keys_to_create) {
  for (const auto& key_to_create : keys_to_create) {
    if (IsValidSymmetricKeyType(key_to_create.second.type))
      return true;
  }

  return false;
}

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case cryptauthv2::KeyType::RAW128:
      return 16u;
    case cryptauthv2::KeyType::RAW256:
      return 32u;
    default:
      NOTREACHED();
      return 0u;
  }
}

// Creates a handle by base64-encoding 32 random bytes.
std::string CreateRandomHandle() {
  std::string bytes(32, '\0');
  crypto::RandBytes(base::WriteInto(&bytes, bytes.size()), bytes.size());

  std::string handle;
  base::Base64Encode(bytes, &handle);

  return handle;
}

}  // namespace

// static
CryptAuthKeyCreatorImpl::Factory*
    CryptAuthKeyCreatorImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthKeyCreatorImpl::Factory* CryptAuthKeyCreatorImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthKeyCreatorImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyCreatorImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthKeyCreator>
CryptAuthKeyCreatorImpl::Factory::BuildInstance() {
  return base::WrapUnique(new CryptAuthKeyCreatorImpl());
}

CryptAuthKeyCreatorImpl::CryptAuthKeyCreatorImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::NewInstance()) {}

void CryptAuthKeyCreatorImpl::CreateKeys(
    const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
        keys_to_create,
    const base::Optional<CryptAuthKey>& server_ephemeral_dh,
    CreateKeysCallback create_keys_callback) {
  DCHECK(!keys_to_create.empty());

  // Fail if CreateKeys() has already been called.
  DCHECK(keys_to_create_.empty() && new_keys_.empty() &&
         !create_keys_callback_);

  keys_to_create_ = keys_to_create;
  server_ephemeral_dh_ = server_ephemeral_dh;
  create_keys_callback_ = std::move(create_keys_callback);

  if (IsClientEphemeralDhKeyNeeded(keys_to_create_)) {
    DCHECK(server_ephemeral_dh_ && server_ephemeral_dh_->IsAsymmetricKey());
    secure_message_delegate_->GenerateKeyPair(
        base::Bind(&CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated,
                   base::Unretained(this)));
    return;
  }

  StartKeyCreation();
}

CryptAuthKeyCreatorImpl::~CryptAuthKeyCreatorImpl() = default;

void CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated(
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(!public_key.empty() && !private_key.empty());

  client_ephemeral_dh_ =
      CryptAuthKey(public_key, private_key, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::P256);

  secure_message_delegate_->DeriveKey(
      client_ephemeral_dh_->private_key(), server_ephemeral_dh_->public_key(),
      base::Bind(
          &CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived,
          base::Unretained(this)));
}

void CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived(
    const std::string& symmetric_key) {
  DCHECK(!symmetric_key.empty());

  dh_handshake_secret_ =
      CryptAuthKey(symmetric_key, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::RAW256);

  StartKeyCreation();
}

void CryptAuthKeyCreatorImpl::StartKeyCreation() {
  for (const auto& key_to_create : keys_to_create_) {
    // If the key to create is symmetric, derive a symmetric key from the
    // Diffie-Hellman handshake secrect using HKDF. The CryptAuth v2
    // Enrollment protocol specifies that the salt should be "CryptAuth
    // Enrollment" and the info should be the key handle. This process is
    // synchronous, unlike SecureMessageDelegate calls.
    if (IsValidSymmetricKeyType(key_to_create.second.type)) {
      std::string handle = key_to_create.second.handle.has_value()
                               ? *key_to_create.second.handle
                               : CreateRandomHandle();
      std::string derived_symmetric_key_material = crypto::HkdfSha256(
          dh_handshake_secret_->symmetric_key(), kSymmetricKeyDerivationSalt,
          handle, NumBytesForSymmetricKeyType(key_to_create.second.type));

      OnSymmetricKeyDerived(key_to_create.first, derived_symmetric_key_material,
                            handle);
    } else {
      DCHECK(IsValidAsymmetricKeyType(key_to_create.second.type));

      secure_message_delegate_->GenerateKeyPair(
          base::Bind(&CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated,
                     base::Unretained(this), key_to_create.first));
    }
  }
}

void CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(keys_to_create_.size() > 0);
  DCHECK(!public_key.empty() && !private_key.empty());

  const CryptAuthKeyCreator::CreateKeyData& create_key_data =
      keys_to_create_.find(bundle_name)->second;

  new_keys_.try_emplace(bundle_name, public_key, private_key,
                        create_key_data.status, create_key_data.type,
                        create_key_data.handle);

  keys_to_create_.erase(bundle_name);
  if (keys_to_create_.empty())
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

void CryptAuthKeyCreatorImpl::OnSymmetricKeyDerived(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& symmetric_key,
    const std::string& handle) {
  DCHECK(keys_to_create_.size() > 0);
  DCHECK(!symmetric_key.empty());

  const CryptAuthKeyCreator::CreateKeyData& create_key_data =
      keys_to_create_.find(bundle_name)->second;

  new_keys_.try_emplace(bundle_name, symmetric_key, create_key_data.status,
                        create_key_data.type, handle);

  keys_to_create_.erase(bundle_name);
  if (keys_to_create_.empty())
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

}  // namespace device_sync

}  // namespace chromeos
