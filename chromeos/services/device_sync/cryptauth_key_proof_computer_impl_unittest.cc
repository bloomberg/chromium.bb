// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "chromeos/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_proof_computer.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

// The salt used for HKDF in symmetric key proofs. This value is part of the
// CryptAuth v2 Enrollment specifications.
const char kSymmetricKeyProofSalt[] = "CryptAuth Key Proof";

const char kFakePublicKeyMaterial[] = "public_key";
const char kFakePrivateKeyMaterial[] = "private_key";
const char kFakePayloadAsymmetricKey[] = "payload_asymmetric_key";

const char kFakeSymmetricKey256Material[] = "symmetric_key_256";
const char kFakePayloadSymmetricKey256[] = "payload_symmetric_key_256";

const char kFakeSymmetricKey128Material[] = "symmetric_key_128";
const char kFakePayloadSymmetricKey128[] = "payload_symmetric_key_128";

void VerifyKeyProofComputationCallback(
    const std::vector<std::string>& expected_key_proofs,
    const std::vector<std::string>& key_proofs) {
  EXPECT_EQ(expected_key_proofs, key_proofs);
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

class CryptAuthKeyProofComputerImplTest : public testing::Test {
 protected:
  CryptAuthKeyProofComputerImplTest() = default;
  ~CryptAuthKeyProofComputerImplTest() override = default;

  void SetUp() override {
    fake_secure_message_delegate_factory_ =
        std::make_unique<FakeSecureMessageDelegateFactory>();
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        fake_secure_message_delegate_factory_.get());

    key_proof_computer_ =
        CryptAuthKeyProofComputerImpl::Factory::Get()->BuildInstance();
  }

  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        nullptr);
  }

  std::string ComputeSymmetricKeyProof(const CryptAuthKey& symmetric_key,
                                       const std::string& payload) {
    size_t num_bytes =
        symmetric_key.type() == cryptauthv2::KeyType::RAW256 ? 32u : 16u;
    std::string derived_symmetric_key_material = crypto::HkdfSha256(
        symmetric_key.symmetric_key(), kSymmetricKeyProofSalt,
        symmetric_key.handle(), num_bytes);

    crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
    std::vector<unsigned char> signed_payload(hmac.DigestLength());
    bool success =
        hmac.Init(derived_symmetric_key_material) &&
        hmac.Sign(payload, signed_payload.data(), signed_payload.size());
    if (!success)
      return "";

    return std::string(signed_payload.begin(), signed_payload.end());
  }

  std::string ComputeAsymmetricKeyProof(const CryptAuthKey& asymmetric_key,
                                        const std::string& payload) {
    multidevice::SecureMessageDelegate::CreateOptions options;
    options.encryption_scheme = securemessage::EncScheme::NONE;
    options.signature_scheme = securemessage::SigScheme::ECDSA_P256_SHA256;
    options.verification_key_id = asymmetric_key.handle();

    std::string key_proof;
    fake_secure_message_delegate()->CreateSecureMessage(
        payload, asymmetric_key.private_key(), options,
        base::Bind(
            [](std::string* key_proof, const std::string& secure_message) {
              *key_proof = secure_message;
            },
            &key_proof));

    return key_proof;
  }

  CryptAuthKeyProofComputer* key_proof_computer() {
    return key_proof_computer_.get();
  }

  multidevice::FakeSecureMessageDelegate* fake_secure_message_delegate() {
    return fake_secure_message_delegate_factory_->instance();
  }

 private:
  std::unique_ptr<FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;

  std::unique_ptr<CryptAuthKeyProofComputer> key_proof_computer_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyProofComputerImplTest);
};

TEST_F(CryptAuthKeyProofComputerImplTest, SuccessfulKeyProofComputation) {
  std::vector<std::pair<CryptAuthKey, std::string>> key_payload_pairs = {
      {CryptAuthKey(kFakePublicKeyMaterial, kFakePrivateKeyMaterial,
                    CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256),
       kFakePayloadAsymmetricKey},
      {CryptAuthKey(kFakeSymmetricKey256Material, CryptAuthKey::Status::kActive,
                    cryptauthv2::KeyType::RAW256),
       kFakePayloadSymmetricKey256},
      {CryptAuthKey(kFakeSymmetricKey128Material, CryptAuthKey::Status::kActive,
                    cryptauthv2::KeyType::RAW128),
       kFakePayloadSymmetricKey128}};

  std::vector<std::string> expected_key_proofs = {
      ComputeAsymmetricKeyProof(key_payload_pairs[0].first,
                                key_payload_pairs[0].second),
      ComputeSymmetricKeyProof(key_payload_pairs[1].first,
                               key_payload_pairs[1].second),
      ComputeSymmetricKeyProof(key_payload_pairs[2].first,
                               key_payload_pairs[2].second)};

  key_proof_computer()->ComputeKeyProofs(
      key_payload_pairs,
      base::BindOnce(&VerifyKeyProofComputationCallback, expected_key_proofs));
}

}  // namespace device_sync

}  // namespace chromeos
