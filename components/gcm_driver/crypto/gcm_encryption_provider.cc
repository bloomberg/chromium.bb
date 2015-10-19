// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_encryption_provider.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "components/gcm_driver/common/gcm_messages.h"
#include "components/gcm_driver/crypto/encryption_header_parsers.h"
#include "components/gcm_driver/crypto/gcm_key_store.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/proto/gcm_encryption_data.pb.h"
#include "crypto/curve25519.h"

namespace gcm {

namespace {

const char kEncryptionProperty[] = "encryption";
const char kEncryptionKeyProperty[] = "encryption_key";

// Directory in the GCM Store in which the encryption database will be stored.
const base::FilePath::CharType kEncryptionDirectoryName[] =
    FILE_PATH_LITERAL("Encryption");

}  // namespace

GCMEncryptionProvider::GCMEncryptionProvider()
    : weak_ptr_factory_(this) {
}

GCMEncryptionProvider::~GCMEncryptionProvider() {
}

void GCMEncryptionProvider::Init(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  DCHECK(!key_store_);

  base::FilePath encryption_store_path = store_path;

  // |store_path| can be empty in tests, which means that the database should
  // be created in memory rather than on-disk.
  if (!store_path.empty())
    encryption_store_path = store_path.Append(kEncryptionDirectoryName);

  key_store_.reset(
      new GCMKeyStore(encryption_store_path, blocking_task_runner));
}

void GCMEncryptionProvider::GetPublicKey(const std::string& app_id,
                                         const PublicKeyCallback& callback) {
  DCHECK(key_store_);
  key_store_->GetKeys(
      app_id, base::Bind(&GCMEncryptionProvider::DidGetPublicKey,
                         weak_ptr_factory_.GetWeakPtr(), app_id, callback));
}

bool GCMEncryptionProvider::IsEncryptedMessage(const IncomingMessage& message)
    const {
  // The Web Push protocol requires the encryption and encryption_key properties
  // to be set, and the raw_data field to be populated with the payload.
  if (message.data.find(kEncryptionProperty) == message.data.end() ||
      message.data.find(kEncryptionKeyProperty) == message.data.end())
    return false;

  // TODO(peter): Support decrypting messages that were sent using the existing
  // GCM protocol, as opposed to the Web Push protocol.

  return message.raw_data.size() > 0;
}

void GCMEncryptionProvider::DecryptMessage(
    const std::string& app_id,
    const IncomingMessage& message,
    const MessageDecryptedCallback& success_callback,
    const DecryptionFailedCallback& failure_callback) {
  DCHECK(key_store_);

  const auto& encryption_header = message.data.find(kEncryptionProperty);
  const auto& encryption_key_header = message.data.find(kEncryptionKeyProperty);

  // Callers are expected to call IsEncryptedMessage() prior to this method.
  DCHECK(encryption_header != message.data.end());
  DCHECK(encryption_key_header != message.data.end());

  std::vector<EncryptionHeaderValues> encryption_header_values;
  if (!ParseEncryptionHeader(encryption_header->second,
                             &encryption_header_values)) {
    DLOG(ERROR) << "Unable to parse the value of the Encryption header";
    failure_callback.Run(DECRYPTION_FAILURE_INVALID_ENCRYPTION_HEADER);
    return;
  }

  if (encryption_header_values.size() != 1u ||
      encryption_header_values[0].salt.size() !=
          GCMMessageCryptographer::kSaltSize) {
    DLOG(ERROR) << "Invalid values supplied in the Encryption header";
    failure_callback.Run(DECRYPTION_FAILURE_INVALID_ENCRYPTION_HEADER);
    return;
  }

  std::vector<EncryptionKeyHeaderValues> encryption_key_header_values;
  if (!ParseEncryptionKeyHeader(encryption_key_header->second,
                                &encryption_key_header_values)) {
    DLOG(ERROR) << "Unable to parse the value of the Encryption-Key header";
    failure_callback.Run(DECRYPTION_FAILURE_INVALID_ENCRYPTION_KEY_HEADER);
    return;
  }

  if (encryption_key_header_values.size() != 1u ||
      encryption_key_header_values[0].dh.size() != crypto::curve25519::kBytes) {
    DLOG(ERROR) << "Invalid values supplied in the Encryption-Key header";
    failure_callback.Run(DECRYPTION_FAILURE_INVALID_ENCRYPTION_KEY_HEADER);
    return;
  }

  key_store_->GetKeys(
      app_id, base::Bind(&GCMEncryptionProvider::DecryptMessageWithKey,
                         weak_ptr_factory_.GetWeakPtr(), message,
                         success_callback, failure_callback,
                         encryption_header_values[0].salt,
                         encryption_key_header_values[0].dh,
                         encryption_header_values[0].rs));
}

void GCMEncryptionProvider::DidGetPublicKey(const std::string& app_id,
                                            const PublicKeyCallback& callback,
                                            const KeyPair& pair) {
  if (!pair.IsInitialized()) {
    key_store_->CreateKeys(
        app_id, base::Bind(&GCMEncryptionProvider::DidCreatePublicKey,
                           weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_CURVE_25519, pair.type());
  callback.Run(pair.public_key());
}

void GCMEncryptionProvider::DidCreatePublicKey(
    const PublicKeyCallback& callback,
    const KeyPair& pair) {
  if (!pair.IsInitialized()) {
    callback.Run(std::string());
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_CURVE_25519, pair.type());
  callback.Run(pair.public_key());
}

void GCMEncryptionProvider::DecryptMessageWithKey(
    const IncomingMessage& message,
    const MessageDecryptedCallback& success_callback,
    const DecryptionFailedCallback& failure_callback,
    const std::string& salt,
    const std::string& dh,
    uint64_t rs,
    const KeyPair& pair) {
  if (!pair.IsInitialized()) {
    DLOG(ERROR) << "Unable to retrieve the keys for the incoming message.";
    failure_callback.Run(DECRYPTION_FAILURE_NO_KEYS);
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_CURVE_25519, pair.type());

  uint8_t shared_key[crypto::curve25519::kBytes];

  // Calculate the shared secret for the message.
  crypto::curve25519::ScalarMult(
      reinterpret_cast<const unsigned char*>(pair.private_key().data()),
      reinterpret_cast<const unsigned char*>(dh.data()),
      shared_key);

  base::StringPiece shared_key_string_piece(
      reinterpret_cast<char*>(shared_key), crypto::curve25519::kBytes);

  std::string plaintext;

  GCMMessageCryptographer cryptographer;
  if (!cryptographer.Decrypt(message.raw_data, shared_key_string_piece, salt,
                             rs, &plaintext)) {
    DLOG(ERROR) << "Unable to decrypt the incoming data.";
    failure_callback.Run(DECRYPTION_FAILURE_INVALID_PAYLOAD);
    return;
  }

  IncomingMessage decrypted_message;
  decrypted_message.collapse_key = message.collapse_key;
  decrypted_message.sender_id = message.sender_id;
  decrypted_message.raw_data.swap(plaintext);
  decrypted_message.decrypted = true;

  // There must be no data associated with the decrypted message at this point,
  // to make sure that we don't end up in an infinite decryption loop.
  DCHECK_EQ(0u, decrypted_message.data.size());

  success_callback.Run(decrypted_message);
}

}  // namespace gcm
