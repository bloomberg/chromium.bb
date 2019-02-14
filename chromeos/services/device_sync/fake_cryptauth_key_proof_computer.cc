// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_key_proof_computer.h"

#include <utility>

namespace chromeos {

namespace device_sync {

FakeCryptAuthKeyProofComputer::FakeCryptAuthKeyProofComputer() = default;

FakeCryptAuthKeyProofComputer::~FakeCryptAuthKeyProofComputer() = default;

void FakeCryptAuthKeyProofComputer::ComputeKeyProofs(
    const std::vector<std::pair<CryptAuthKey, std::string>>& key_payload_pairs,
    ComputeKeyProofsCallback compute_key_proofs_callback) {
  DCHECK(!key_payload_pairs.empty());
  DCHECK(key_payload_pairs_.empty());
  key_payload_pairs_ = key_payload_pairs;
  compute_key_proofs_callback_ = std::move(compute_key_proofs_callback);
}

}  // namespace device_sync

}  // namespace chromeos
