// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/cryptauth_key.h"

namespace chromeos {

namespace device_sync {

// The lone method ComputeKeyProofs() takes a list of key-payload pairs and
// returns a list of key proof strings, ordered consistent with the
// input list. The key proofs are used by CryptAuth to verify that the client
// has the appropriate key material.
//
// The CryptAuth v2 Enrollment protocol requires the following key proof
// formats:
//   - Symmetric keys: The HMAC-SHA256 of the payload, using a key derived from
//     the symmetric key. Specifically,
//         HMAC(HKDF(|key|, salt="CryptAuth Key Proof", info=|key_handle|),
//              |payload|)
//
//   - Asymmetric keys: A signed, unencrypted, and serialized SecureMessage
//     proto, with the following parameters:
//         - signature_scheme = ECDSA_P256_SHA256,
//         - encryption_scheme = NONE,
//         - verification_key_id = |key_handle|,
//         - body = |payload|,
//     signed with the private key material.
//
// The protocol also demands that the |random_session_id|, sent by the CryptAuth
// server via the SyncKeysResponse, be used as the payload for key proofs. In
// the future, key crossproofs might be employed, where the payload will
// consist of other key proofs.
//
// Requirements:
//   - An instance of this class should only be used once.
//   - The input list, |key_payload_pairs|, cannot be empty.
//   - Currently, the only supported key types are RAW128 and RAW256 for
//     symmetric keys and P256 for asymmetric keys.
class CryptAuthKeyProofComputer {
 public:
  CryptAuthKeyProofComputer() = default;
  virtual ~CryptAuthKeyProofComputer() = default;

  using ComputeKeyProofsCallback =
      base::OnceCallback<void(const std::vector<std::string>& /* key_proofs*/)>;
  virtual void ComputeKeyProofs(
      const std::vector<std::pair<CryptAuthKey, std::string>>&
          key_payload_pairs,
      ComputeKeyProofsCallback compute_key_proofs_callback) = 0;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyProofComputer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
