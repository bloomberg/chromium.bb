// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {

namespace device_sync {

// Encrypts/decrypts strings using the Elliptic Curve Integrated Encryption
// Scheme (ECIES).
//
// A CryptAuthEciesEncryptor object is designed to be used for only one method
// call. To perform another encryption or decryption, a new object should be
// created.
class CryptAuthEciesEncryptor {
 public:
  using IdToInputMap = base::flat_map<std::string, std::string>;
  using IdToOutputMap =
      base::flat_map<std::string, base::Optional<std::string>>;
  using SingleInputCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;
  using BatchCallback = base::OnceCallback<void(const IdToOutputMap&)>;

  virtual ~CryptAuthEciesEncryptor();

  // Encrypts |unencrypted_payload| with |encrypting_public_key|, returning the
  // encrypted payload in the callback or null if the encryption was
  // unsuccessful.
  void Encrypt(const std::string& unencrypted_payload,
               const std::string& encrypting_public_key,
               SingleInputCallback encryption_finished_callback);

  // Decrypts |encrypted_payload| with |decrypting_private_key|, returning the
  // decrypted payload in the callback or null if the decryption was
  // unsuccessful.
  void Decrypt(const std::string& encrypted_payload,
               const std::string& decrypting_private_key,
               SingleInputCallback decryption_finished_callback);

  // Encrypts all values of the input ID to payload map,
  // |id_to_unencrypted_payload_map|, using the same |encrypting_public_key|.
  // The encrypted payloads are returned in the callback, paired with their
  // input IDs. Unsuccessful encryptions result in null values. Note: The IDs
  // are only used to correlate the output with input.
  void BatchEncrypt(const IdToInputMap& id_to_unencrypted_payload_map,
                    const std::string& encrypting_public_key,
                    BatchCallback encryption_finished_callback);

  // Decrypts all values of the input ID to payload map,
  // |id_to_encrypted_payload_map|, using the same |decrypting_private_key|.
  // The decrypted payloads are returned in the callback, paired with their
  // input IDs. Unsuccessful decryptions result in null values. Note: The IDs
  // are only used to correlate the output with input.
  void BatchDecrypt(const IdToInputMap& id_to_encrypted_payload_map,
                    const std::string& decrypting_private_key,
                    BatchCallback decryption_finished_callback);

 protected:
  CryptAuthEciesEncryptor();

  virtual void OnBatchEncryptionStarted() = 0;
  virtual void OnBatchDecryptionStarted() = 0;

  void OnAttemptFinished(const IdToOutputMap& id_to_output_map);

  IdToInputMap id_to_input_map_;
  std::string input_key_;

 private:
  void ProcessInput(const IdToInputMap& id_to_input_map,
                    const std::string& input_key,
                    BatchCallback callback);

  BatchCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEciesEncryptor);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_
