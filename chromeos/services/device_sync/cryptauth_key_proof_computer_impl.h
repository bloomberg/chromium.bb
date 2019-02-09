// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_

#include "chromeos/services/device_sync/cryptauth_key_proof_computer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace device_sync {

class CryptAuthKeyProofComputerImpl : public CryptAuthKeyProofComputer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyProofComputer> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthKeyProofComputerImpl() override;

  // CryptAuthKeyProofComputer:
  void ComputeKeyProofs(
      const std::vector<std::pair<CryptAuthKey, std::string>>&
          key_payload_pairs,
      ComputeKeyProofsCallback compute_key_proofs_callback) override;

 private:
  CryptAuthKeyProofComputerImpl();

  void ComputeSymmetricKeyProof(const size_t index,
                                const CryptAuthKey& symmetric_key,
                                const std::string& payload);
  void ComputeAsymmetricKeyProof(const size_t index,
                                 const CryptAuthKey& asymmetric_key,
                                 const std::string& payload);
  void OnKeyProofComputed(const size_t index,
                          const std::string& single_key_proof);

  size_t num_key_proofs_to_compute_ = 0;
  std::vector<std::pair<CryptAuthKey, std::string>> key_payload_pairs_;
  std::vector<std::string> output_key_proofs_;
  ComputeKeyProofsCallback compute_key_proofs_callback_;

  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyProofComputerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
