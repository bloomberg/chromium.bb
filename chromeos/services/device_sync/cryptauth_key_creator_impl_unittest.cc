// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_creator_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_creator.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

// The salt used in HKDF to derive symmetric keys from Diffie-Hellman handshake.
// This value is part of the CryptAuth v2 Enrollment specifications.
const char kSymmetricKeyDerivationSalt[] = "CryptAuth Enrollment";

const char kFakeServerEphemeralDhPublicKeyMaterial[] = "server_ephemeral_dh";
const char kFakeClientEphemeralDhPublicKeyMaterial[] = "client_ephemeral_dh";
const char kFakeAsymmetricKeyHandle[] = "asymmetric_key_handle";
const char kFakePublicKeyMaterial[] = "public_key";
const char kFakeSymmetricKeyHandle[] = "symmetric_key_handle";

void VerifyCreatedKeysCallback(
    const base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>&
        expected_new_keys,
    const base::Optional<CryptAuthKey>& expected_client_ephemeral_dh,
    const base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>& new_keys,
    const base::Optional<CryptAuthKey>& client_ephemeral_dh) {
  EXPECT_EQ(expected_client_ephemeral_dh, client_ephemeral_dh);
  EXPECT_EQ(expected_new_keys, new_keys);
}

class FakeSecureMessageDelegateFactory
    : public multidevice::SecureMessageDelegateImpl::Factory {
 public:
  FakeSecureMessageDelegateFactory() = default;

  ~FakeSecureMessageDelegateFactory() override = default;

  multidevice::FakeSecureMessageDelegate* instance() { return instance_; }

 private:
  // multidevice::SecureMessageDelegateImpl::Factory:
  std::unique_ptr<multidevice::SecureMessageDelegate> BuildInstance() override {
    auto instance = std::make_unique<multidevice::FakeSecureMessageDelegate>();
    instance_ = instance.get();

    return instance;
  }

  multidevice::FakeSecureMessageDelegate* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureMessageDelegateFactory);
};

}  // namespace

class CryptAuthKeyCreatorImplTest : public testing::Test {
 protected:
  CryptAuthKeyCreatorImplTest() = default;
  ~CryptAuthKeyCreatorImplTest() override = default;

  void SetUp() override {
    fake_secure_message_delegate_factory_ =
        std::make_unique<FakeSecureMessageDelegateFactory>();
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        fake_secure_message_delegate_factory_.get());

    key_creator_ = CryptAuthKeyCreatorImpl::Factory::Get()->BuildInstance();
  }

  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        nullptr);
  }

  CryptAuthKey DeriveSecret(
      const base::Optional<CryptAuthKey>& server_ephemeral_dh,
      const base::Optional<CryptAuthKey>& client_ephemeral_dh) {
    std::string derived_key;
    fake_secure_message_delegate()->DeriveKey(
        client_ephemeral_dh->private_key(), server_ephemeral_dh->public_key(),
        base::Bind([](std::string* derived_key,
                      const std::string& key) { *derived_key = key; },
                   &derived_key));
    return CryptAuthKey(derived_key, CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::RAW256);
  }

  CryptAuthKeyCreator* key_creator() { return key_creator_.get(); }

  multidevice::FakeSecureMessageDelegate* fake_secure_message_delegate() {
    return fake_secure_message_delegate_factory_->instance();
  }

 private:
  std::unique_ptr<FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;

  std::unique_ptr<CryptAuthKeyCreator> key_creator_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyCreatorImplTest);
};

TEST_F(CryptAuthKeyCreatorImplTest, AsymmetricKeyCreation) {
  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      keys_to_create = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           CryptAuthKeyCreator::CreateKeyData(CryptAuthKey::Status::kActive,
                                              cryptauthv2::KeyType::P256,
                                              kFakeAsymmetricKeyHandle)}};

  CryptAuthKey expected_asymmetric_key(
      kFakePublicKeyMaterial,
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
          kFakePublicKeyMaterial),
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
      kFakeAsymmetricKeyHandle);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> expected_new_keys = {
      {CryptAuthKeyBundle::Name::kUserKeyPair, expected_asymmetric_key}};

  fake_secure_message_delegate()->set_next_public_key(kFakePublicKeyMaterial);

  key_creator()->CreateKeys(
      keys_to_create, base::nullopt /* fake_server_ephemeral_dh */,
      base::BindOnce(VerifyCreatedKeysCallback, expected_new_keys,
                     base::nullopt /* expected_client_ephemeral_dh */));
}

TEST_F(CryptAuthKeyCreatorImplTest, SymmetricKeyCreation) {
  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      keys_to_create = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           CryptAuthKeyCreator::CreateKeyData(CryptAuthKey::Status::kActive,
                                              cryptauthv2::KeyType::RAW256,
                                              kFakeSymmetricKeyHandle)}};

  base::Optional<CryptAuthKey> fake_server_ephemeral_dh =
      CryptAuthKey(kFakeServerEphemeralDhPublicKeyMaterial,
                   fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
                       kFakeServerEphemeralDhPublicKeyMaterial),
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  base::Optional<CryptAuthKey> expected_client_ephemeral_dh =
      CryptAuthKey(kFakeClientEphemeralDhPublicKeyMaterial,
                   fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
                       kFakeClientEphemeralDhPublicKeyMaterial),
                   CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  CryptAuthKey expected_handshake_secret =
      DeriveSecret(fake_server_ephemeral_dh, expected_client_ephemeral_dh);
  std::string expected_symmetric_key_material = crypto::HkdfSha256(
      expected_handshake_secret.symmetric_key(), kSymmetricKeyDerivationSalt,
      kFakeSymmetricKeyHandle, 32u /* derived_key_size */);

  CryptAuthKey expected_symmetric_key(
      expected_symmetric_key_material, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::RAW256, kFakeSymmetricKeyHandle);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> expected_new_keys = {
      {CryptAuthKeyBundle::Name::kUserKeyPair, expected_symmetric_key}};

  fake_secure_message_delegate()->set_next_public_key(
      kFakeClientEphemeralDhPublicKeyMaterial);

  key_creator()->CreateKeys(
      keys_to_create, fake_server_ephemeral_dh,
      base::BindOnce(VerifyCreatedKeysCallback, expected_new_keys,
                     expected_client_ephemeral_dh));
}

}  // namespace device_sync

}  // namespace chromeos
