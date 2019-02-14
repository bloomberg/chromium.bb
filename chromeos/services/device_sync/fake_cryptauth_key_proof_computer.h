// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_proof_computer.h"

namespace chromeos {

namespace device_sync {

class FakeCryptAuthKeyProofComputer : public CryptAuthKeyProofComputer {
 public:
  FakeCryptAuthKeyProofComputer();
  ~FakeCryptAuthKeyProofComputer() override;

  // CryptAuthKeyProofComputer:
  void ComputeKeyProofs(
      const std::vector<std::pair<CryptAuthKey, std::string>>&
          key_payload_pairs,
      ComputeKeyProofsCallback compute_key_proofs_callback) override;

  const std::vector<std::pair<CryptAuthKey, std::string>>& key_payload_pairs()
      const {
    return key_payload_pairs_;
  }

  ComputeKeyProofsCallback& compute_key_proofs_callback() {
    return compute_key_proofs_callback_;
  }

 private:
  std::vector<std::pair<CryptAuthKey, std::string>> key_payload_pairs_;
  ComputeKeyProofsCallback compute_key_proofs_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthKeyProofComputer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
