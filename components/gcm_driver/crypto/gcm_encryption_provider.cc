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
#include "components/gcm_driver/crypto/message_payload_parser.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/crypto/proto/gcm_encryption_data.pb.h"

namespace gcm {

namespace {

const char kEncryptionProperty[] = "encryption";
const char kContentEncodingProperty[] = "content-encoding";
const char kCryptoKeyProperty[] = "crypto-key";

// Content coding name defined by ietf-httpbis-encryption-encoding.
const char kContentCodingAes128Gcm[] = "aes128gcm";

// Directory in the GCM Store in which the encryption database will be stored.
const base::FilePath::CharType kEncryptionDirectoryName[] =
    FILE_PATH_LITERAL("Encryption");

}  // namespace

std::string GCMEncryptionProvider::ToDecryptionResultDetailsString(
    DecryptionResult result) {
  switch (result) {
    case DECRYPTION_RESULT_UNENCRYPTED:
      return "Message was not encrypted";
    case DECRYPTION_RESULT_DECRYPTED_DRAFT_03:
      return "Message decrypted (draft 03)";
    case DECRYPTION_RESULT_DECRYPTED_DRAFT_08:
      return "Message decrypted (draft 08)";
    case DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER:
      return "Invalid format for the Encryption header";
    case DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER:
      return "Invalid format for the Crypto-Key header";
    case DECRYPTION_RESULT_NO_KEYS:
      return "There are no associated keys with the subscription";
    case DECRYPTION_RESULT_INVALID_SHARED_SECRET:
      return "The shared secret cannot be derived from the keying material";
    case DECRYPTION_RESULT_INVALID_PAYLOAD:
      return "AES-GCM decryption failed";
    case DECRYPTION_RESULT_INVALID_BINARY_HEADER:
      return "The message's binary header could not be parsed.";
  }

  NOTREACHED();
  return "(invalid result)";
}

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

void GCMEncryptionProvider::GetEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    const EncryptionInfoCallback& callback) {
  DCHECK(key_store_);
  key_store_->GetKeys(app_id, authorized_entity,
                      false /* fallback_to_empty_authorized_entity */,
                      base::Bind(&GCMEncryptionProvider::DidGetEncryptionInfo,
                                 weak_ptr_factory_.GetWeakPtr(), app_id,
                                 authorized_entity, callback));
}

void GCMEncryptionProvider::RemoveEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    const base::Closure& callback) {
  DCHECK(key_store_);
  key_store_->RemoveKeys(app_id, authorized_entity, callback);
}

bool GCMEncryptionProvider::IsEncryptedMessage(const IncomingMessage& message)
    const {
  // Messages that explicitly specify their content coding to be "aes128gcm"
  // indicate that they use draft-ietf-webpush-encryption-08.
  auto content_encoding_iter = message.data.find(kContentEncodingProperty);
  if (content_encoding_iter != message.data.end() &&
      content_encoding_iter->second == kContentCodingAes128Gcm) {
    return true;
  }

  // The Web Push protocol requires the encryption and crypto-key properties to
  // be set, and the raw_data field to be populated with the payload.
  if (message.data.find(kEncryptionProperty) == message.data.end() ||
      message.data.find(kCryptoKeyProperty) == message.data.end())
    return false;

  return message.raw_data.size() > 0;
}

void GCMEncryptionProvider::DecryptMessage(
    const std::string& app_id,
    const IncomingMessage& message,
    const MessageCallback& callback) {
  DCHECK(key_store_);
  if (!IsEncryptedMessage(message)) {
    callback.Run(DECRYPTION_RESULT_UNENCRYPTED, message);
    return;
  }

  std::string salt, public_key, ciphertext;
  GCMMessageCryptographer::Version version;
  uint32_t record_size;

  auto content_encoding_iter = message.data.find(kContentEncodingProperty);
  if (content_encoding_iter != message.data.end() &&
      content_encoding_iter->second == kContentCodingAes128Gcm) {
    // The message follows encryption per draft-ietf-webpush-encryption-08. Use
    // the binary header of the message to derive the values.

    MessagePayloadParser parser(message.raw_data);
    if (!parser.IsValid()) {
      DLOG(ERROR) << "Unable to parse the message's binary header";
      callback.Run(DECRYPTION_RESULT_INVALID_BINARY_HEADER, IncomingMessage());
      return;
    }

    salt = parser.salt();
    public_key = parser.public_key();
    record_size = parser.record_size();
    ciphertext = parser.ciphertext();
    version = GCMMessageCryptographer::Version::DRAFT_08;

  } else {
    // The message follows encryption per draft-ietf-webpush-encryption-03. Use
    // the Encryption and Crypto-Key header values to derive the values.

    const auto& encryption_header = message.data.find(kEncryptionProperty);
    DCHECK(encryption_header != message.data.end());

    const auto& crypto_key_header = message.data.find(kCryptoKeyProperty);
    DCHECK(crypto_key_header != message.data.end());

    EncryptionHeaderIterator encryption_header_iterator(
        encryption_header->second.begin(), encryption_header->second.end());
    if (!encryption_header_iterator.GetNext()) {
      DLOG(ERROR) << "Unable to parse the value of the Encryption header";
      callback.Run(DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER,
                   IncomingMessage());
      return;
    }

    if (encryption_header_iterator.salt().size() !=
        GCMMessageCryptographer::kSaltSize) {
      DLOG(ERROR) << "Invalid values supplied in the Encryption header";
      callback.Run(DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER,
                   IncomingMessage());
      return;
    }

    salt = encryption_header_iterator.salt();
    record_size = encryption_header_iterator.rs();

    CryptoKeyHeaderIterator crypto_key_header_iterator(
        crypto_key_header->second.begin(), crypto_key_header->second.end());
    if (!crypto_key_header_iterator.GetNext()) {
      DLOG(ERROR) << "Unable to parse the value of the Crypto-Key header";
      callback.Run(DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER,
                   IncomingMessage());
      return;
    }

    // Ignore values that don't include the "dh" property. When using VAPID, it
    // is valid for the application server to supply multiple values.
    while (crypto_key_header_iterator.dh().empty() &&
           crypto_key_header_iterator.GetNext()) {
    }

    bool valid_crypto_key_header = false;

    if (!crypto_key_header_iterator.dh().empty()) {
      public_key = crypto_key_header_iterator.dh();
      valid_crypto_key_header = true;

      // Guard against the "dh" property being included more than once.
      while (crypto_key_header_iterator.GetNext()) {
        if (crypto_key_header_iterator.dh().empty())
          continue;

        valid_crypto_key_header = false;
        break;
      }
    }

    if (!valid_crypto_key_header) {
      DLOG(ERROR) << "Invalid values supplied in the Crypto-Key header";
      callback.Run(DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER,
                   IncomingMessage());
      return;
    }

    ciphertext = message.raw_data;
    version = GCMMessageCryptographer::Version::DRAFT_03;
  }

  // Use |fallback_to_empty_authorized_entity|, since this message might have
  // been sent to either an InstanceID token or a non-InstanceID registration.
  key_store_->GetKeys(
      app_id, message.sender_id /* authorized_entity */,
      true /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMEncryptionProvider::DecryptMessageWithKey,
                 weak_ptr_factory_.GetWeakPtr(), message.collapse_key,
                 message.sender_id, std::move(salt), std::move(public_key),
                 record_size, std::move(ciphertext), version, callback));
}

void GCMEncryptionProvider::DidGetEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    const EncryptionInfoCallback& callback,
    const KeyPair& pair,
    const std::string& auth_secret) {
  if (!pair.IsInitialized()) {
    key_store_->CreateKeys(
        app_id, authorized_entity,
        base::Bind(&GCMEncryptionProvider::DidCreateEncryptionInfo,
                   weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_P256, pair.type());
  callback.Run(pair.public_key(), auth_secret);
}

void GCMEncryptionProvider::DidCreateEncryptionInfo(
    const EncryptionInfoCallback& callback,
    const KeyPair& pair,
    const std::string& auth_secret) {
  if (!pair.IsInitialized()) {
    callback.Run(std::string() /* p256dh */,
                 std::string() /* auth_secret */);
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_P256, pair.type());
  callback.Run(pair.public_key(), auth_secret);
}

void GCMEncryptionProvider::DecryptMessageWithKey(
    const std::string& collapse_key,
    const std::string& sender_id,
    const std::string& salt,
    const std::string& public_key,
    uint32_t record_size,
    const std::string& ciphertext,
    GCMMessageCryptographer::Version version,
    const MessageCallback& callback,
    const KeyPair& pair,
    const std::string& auth_secret) {
  if (!pair.IsInitialized()) {
    DLOG(ERROR) << "Unable to retrieve the keys for the incoming message.";
    callback.Run(DECRYPTION_RESULT_NO_KEYS, IncomingMessage());
    return;
  }

  DCHECK_EQ(KeyPair::ECDH_P256, pair.type());

  std::string shared_secret;
  if (!ComputeSharedP256Secret(pair.private_key(), pair.public_key_x509(),
                               public_key, &shared_secret)) {
    DLOG(ERROR) << "Unable to calculate the shared secret.";
    callback.Run(DECRYPTION_RESULT_INVALID_SHARED_SECRET, IncomingMessage());
    return;
  }

  std::string plaintext;

  GCMMessageCryptographer cryptographer(version);

  if (!cryptographer.Decrypt(pair.public_key(), public_key, shared_secret,
                             auth_secret, salt, ciphertext, record_size,
                             &plaintext)) {
    DLOG(ERROR) << "Unable to decrypt the incoming data.";
    callback.Run(DECRYPTION_RESULT_INVALID_PAYLOAD, IncomingMessage());
    return;
  }

  IncomingMessage decrypted_message;
  decrypted_message.collapse_key = collapse_key;
  decrypted_message.sender_id = sender_id;
  decrypted_message.raw_data.swap(plaintext);
  decrypted_message.decrypted = true;

  // There must be no data associated with the decrypted message at this point,
  // to make sure that we don't end up in an infinite decryption loop.
  DCHECK_EQ(0u, decrypted_message.data.size());

  callback.Run(version == GCMMessageCryptographer::Version::DRAFT_03
                   ? DECRYPTION_RESULT_DECRYPTED_DRAFT_03
                   : DECRYPTION_RESULT_DECRYPTED_DRAFT_08,
               decrypted_message);
}

}  // namespace gcm
