// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/secure_message_delegate.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"

namespace chromeos {

namespace device_sync {

namespace {

// The salt used for HKDF in symmetric key proofs. This value is part of the
// CryptAuth v2 Enrollment specifications.
const char kSymmetricKeyProofSalt[] = "CryptAuth Key Proof";

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case (cryptauthv2::KeyType::RAW128):
      return 16u;
    case (cryptauthv2::KeyType::RAW256):
      return 32u;
    default:
      NOTREACHED();
      return 0u;
  }
}

bool IsValidAsymmetricKey(const CryptAuthKey& key) {
  return key.IsAsymmetricKey() && !key.private_key().empty() &&
         key.type() == cryptauthv2::KeyType::P256;
}

}  // namespace

// static
CryptAuthKeyProofComputerImpl::Factory*
    CryptAuthKeyProofComputerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthKeyProofComputerImpl::Factory*
CryptAuthKeyProofComputerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthKeyProofComputerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthKeyProofComputerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyProofComputerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthKeyProofComputer>
CryptAuthKeyProofComputerImpl::Factory::BuildInstance() {
  return base::WrapUnique(new CryptAuthKeyProofComputerImpl());
}

CryptAuthKeyProofComputerImpl::CryptAuthKeyProofComputerImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::NewInstance()) {}

CryptAuthKeyProofComputerImpl::~CryptAuthKeyProofComputerImpl() = default;

void CryptAuthKeyProofComputerImpl::ComputeKeyProofs(
    const std::vector<std::pair<CryptAuthKey, std::string>>& key_payload_pairs,
    ComputeKeyProofsCallback compute_key_proofs_callback) {
  DCHECK(!key_payload_pairs.empty());

  // Fail if ComputeKeyProofs() has already been called.
  DCHECK(num_key_proofs_to_compute_ == 0 && key_payload_pairs_.empty() &&
         output_key_proofs_.empty() && !compute_key_proofs_callback_);

  num_key_proofs_to_compute_ = key_payload_pairs.size();
  key_payload_pairs_ = key_payload_pairs;
  output_key_proofs_.resize(num_key_proofs_to_compute_);
  compute_key_proofs_callback_ = std::move(compute_key_proofs_callback);

  for (size_t i = 0; i < key_payload_pairs_.size(); ++i) {
    if (key_payload_pairs_[i].first.IsSymmetricKey()) {
      ComputeSymmetricKeyProof(i, key_payload_pairs_[i].first,
                               key_payload_pairs_[i].second);
    } else {
      DCHECK(IsValidAsymmetricKey(key_payload_pairs_[i].first));
      ComputeAsymmetricKeyProof(i, key_payload_pairs_[i].first,
                                key_payload_pairs_[i].second);
    }
  }
}

void CryptAuthKeyProofComputerImpl::ComputeSymmetricKeyProof(
    const size_t index,
    const CryptAuthKey& symmetric_key,
    const std::string& payload) {
  std::string derived_symmetric_key_material =
      crypto::HkdfSha256(symmetric_key.symmetric_key(), kSymmetricKeyProofSalt,
                         symmetric_key.handle(),
                         NumBytesForSymmetricKeyType(symmetric_key.type()));

  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  std::vector<unsigned char> signed_payload(hmac.DigestLength());
  bool success =
      hmac.Init(derived_symmetric_key_material) &&
      hmac.Sign(payload, signed_payload.data(), signed_payload.size());
  DCHECK(success);

  OnKeyProofComputed(index,
                     std::string(signed_payload.begin(), signed_payload.end()));
}

void CryptAuthKeyProofComputerImpl::ComputeAsymmetricKeyProof(
    const size_t index,
    const CryptAuthKey& asymmetric_key,
    const std::string& payload) {
  multidevice::SecureMessageDelegate::CreateOptions options;
  options.encryption_scheme = securemessage::EncScheme::NONE;
  options.signature_scheme = securemessage::SigScheme::ECDSA_P256_SHA256;
  options.verification_key_id = asymmetric_key.handle();

  secure_message_delegate_->CreateSecureMessage(
      payload, asymmetric_key.private_key(), options,
      base::Bind(&CryptAuthKeyProofComputerImpl::OnKeyProofComputed,
                 base::Unretained(this), index));
}

void CryptAuthKeyProofComputerImpl::OnKeyProofComputed(
    const size_t index,
    const std::string& key_proof) {
  DCHECK(index < output_key_proofs_.size());
  output_key_proofs_[index] = key_proof;

  --num_key_proofs_to_compute_;
  if (!num_key_proofs_to_compute_)
    std::move(compute_key_proofs_callback_).Run(output_key_proofs_);
}

}  // namespace device_sync

}  // namespace chromeos
