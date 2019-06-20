// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_ecies_encryptor.h"

#include <utility>

#include "base/bind.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kSinglePayloadId[] = "single_payload_id";

void ForwardResultToSingleInputCallback(
    CryptAuthEciesEncryptor::SingleInputCallback single_input_callback,
    const CryptAuthEciesEncryptor::IdToOutputMap& id_to_output_map) {
  const auto it = id_to_output_map.find(kSinglePayloadId);
  DCHECK(it != id_to_output_map.end());

  std::move(single_input_callback).Run(it->second);
}

}  // namespace

CryptAuthEciesEncryptor::CryptAuthEciesEncryptor() = default;

CryptAuthEciesEncryptor::~CryptAuthEciesEncryptor() = default;

void CryptAuthEciesEncryptor::Encrypt(
    const std::string& unencrypted_payload,
    const std::string& encrypting_public_key,
    SingleInputCallback encryption_finished_callback) {
  BatchEncrypt({{kSinglePayloadId, unencrypted_payload}}, encrypting_public_key,
               base::BindOnce(&ForwardResultToSingleInputCallback,
                              std::move(encryption_finished_callback)));
}

void CryptAuthEciesEncryptor::Decrypt(
    const std::string& encrypted_payload,
    const std::string& decrypting_private_key,
    SingleInputCallback decryption_finished_callback) {
  BatchDecrypt({{kSinglePayloadId, encrypted_payload}}, decrypting_private_key,
               base::BindOnce(&ForwardResultToSingleInputCallback,
                              std::move(decryption_finished_callback)));
}

void CryptAuthEciesEncryptor::BatchEncrypt(
    const IdToInputMap& id_to_unencrypted_payload_map,
    const std::string& encrypting_public_key,
    BatchCallback encryption_finished_callback) {
  ProcessInput(id_to_unencrypted_payload_map, encrypting_public_key,
               std::move(encryption_finished_callback));

  OnBatchEncryptionStarted();
}

void CryptAuthEciesEncryptor::BatchDecrypt(
    const IdToInputMap& id_to_encrypted_payload_map,
    const std::string& decrypting_private_key,
    BatchCallback decryption_finished_callback) {
  ProcessInput(id_to_encrypted_payload_map, decrypting_private_key,
               std::move(decryption_finished_callback));

  OnBatchDecryptionStarted();
}

void CryptAuthEciesEncryptor::OnAttemptFinished(
    const IdToOutputMap& id_to_output_map) {
  std::move(callback_).Run(id_to_output_map);
}

void CryptAuthEciesEncryptor::ProcessInput(const IdToInputMap& id_to_input_map,
                                           const std::string& input_key,
                                           BatchCallback callback) {
  DCHECK(!id_to_input_map.empty());
  DCHECK(!input_key.empty());
  DCHECK(callback);

  // Fail if a public method has already been called.
  DCHECK(id_to_input_map_.empty());

  id_to_input_map_ = id_to_input_map;
  input_key_ = input_key;
  callback_ = std::move(callback);
}

}  // namespace device_sync

}  // namespace chromeos
