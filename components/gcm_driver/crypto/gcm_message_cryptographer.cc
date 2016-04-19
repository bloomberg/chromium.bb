// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <algorithm>
#include <sstream>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "crypto/hkdf.h"

namespace gcm {
namespace {

// Size, in bytes, of the nonce for a record. This must be at least the size
// of a uint64_t, which is used to indicate the record sequence number.
const uint64_t kNonceSize = 12;

// The default record size as defined by draft-thomson-http-encryption.
const size_t kDefaultRecordSize = 4096;

// Key size, in bytes, of a valid AEAD_AES_128_GCM key.
const size_t kContentEncryptionKeySize = 16;

// Creates the info parameter for an HKDF value for the given |content_encoding|
// in accordance with draft-thomson-http-encryption.
//
// cek_info = "Content-Encoding: aesgcm" || 0x00 || context
// nonce_info = "Content-Encoding: nonce" || 0x00 || context
//
// context = label || 0x00 ||
//           length(recipient_public) || recipient_public ||
//           length(sender_public) || sender_public
//
// The length of the public keys must be written as a two octet unsigned integer
// in network byte order (big endian).
std::string InfoForContentEncoding(
    const char* content_encoding,
    GCMMessageCryptographer::Label label,
    const base::StringPiece& recipient_public_key,
    const base::StringPiece& sender_public_key) {
  DCHECK(GCMMessageCryptographer::Label::P256 == label);
  DCHECK_EQ(recipient_public_key.size(), 65u);
  DCHECK_EQ(sender_public_key.size(), 65u);

  std::stringstream info_stream;
  info_stream << "Content-Encoding: " << content_encoding << '\x00';

  switch (label) {
    case GCMMessageCryptographer::Label::P256:
      info_stream << "P-256" << '\x00';
      break;
  }

  uint16_t local_len =
      base::HostToNet16(static_cast<uint16_t>(recipient_public_key.size()));
  info_stream.write(reinterpret_cast<char*>(&local_len), sizeof(local_len));
  info_stream << recipient_public_key;

  uint16_t peer_len =
      base::HostToNet16(static_cast<uint16_t>(sender_public_key.size()));
  info_stream.write(reinterpret_cast<char*>(&peer_len), sizeof(peer_len));
  info_stream << sender_public_key;

  return info_stream.str();
}

}  // namespace

const size_t GCMMessageCryptographer::kAuthenticationTagBytes = 16;
const size_t GCMMessageCryptographer::kSaltSize = 16;

GCMMessageCryptographer::GCMMessageCryptographer(
    Label label,
    const base::StringPiece& recipient_public_key,
    const base::StringPiece& sender_public_key,
    const std::string& auth_secret)
    : content_encryption_key_info_(
          InfoForContentEncoding("aesgcm", label, recipient_public_key,
                                 sender_public_key)),
      nonce_info_(
          InfoForContentEncoding("nonce", label, recipient_public_key,
                                 sender_public_key)),
      auth_secret_(auth_secret) {
}

GCMMessageCryptographer::~GCMMessageCryptographer() {}

bool GCMMessageCryptographer::Encrypt(const base::StringPiece& plaintext,
                                      const base::StringPiece& ikm,
                                      const base::StringPiece& salt,
                                      size_t* record_size,
                                      std::string* ciphertext) const {
  DCHECK(ciphertext);
  DCHECK(record_size);

  if (salt.size() != kSaltSize)
    return false;

  std::string prk = DerivePseudoRandomKey(ikm);

  std::string content_encryption_key = DeriveContentEncryptionKey(prk, salt);
  std::string nonce = DeriveNonce(prk, salt);

  // Prior to the plaintext, draft-thomson-http-encryption has a two-byte
  // padding length followed by zero to 65535 bytes of padding. There is no need
  // for payloads created by Chrome to be padded so the padding length is set to
  // zero.
  std::string record;
  record.reserve(sizeof(uint16_t) + plaintext.size());
  record.append(sizeof(uint16_t), '\0');

  plaintext.AppendToString(&record);

  std::string encrypted_record;
  if (!EncryptDecryptRecordInternal(ENCRYPT, record, content_encryption_key,
                                    nonce, &encrypted_record)) {
    return false;
  }

  // The advertised record size must be at least one more than the padded
  // plaintext to ensure only one record.
  *record_size = std::max(kDefaultRecordSize, record.size() + 1);

  ciphertext->swap(encrypted_record);
  return true;
}

bool GCMMessageCryptographer::Decrypt(const base::StringPiece& ciphertext,
                                      const base::StringPiece& ikm,
                                      const base::StringPiece& salt,
                                      size_t record_size,
                                      std::string* plaintext) const {
  DCHECK(plaintext);

  if (salt.size() != kSaltSize || record_size <= 1)
    return false;

  // The |ciphertext| must be at least of size kAuthenticationTagBytes plus
  // len(uint16) to hold the message's padding length, which is the case when an
  // empty message with a zero padding length has been received. Per
  // https://tools.ietf.org/html/draft-thomson-http-encryption-02#section-3, the
  // |record_size| parameter must be large enough to use only one record.
  if (ciphertext.size() < sizeof(uint16_t) + kAuthenticationTagBytes ||
      ciphertext.size() > record_size + kAuthenticationTagBytes) {
    return false;
  }

  std::string prk = DerivePseudoRandomKey(ikm);

  std::string content_encryption_key = DeriveContentEncryptionKey(prk, salt);
  std::string nonce = DeriveNonce(prk, salt);

  std::string decrypted_record_string;
  if (!EncryptDecryptRecordInternal(DECRYPT, ciphertext, content_encryption_key,
                                    nonce, &decrypted_record_string)) {
    return false;
  }

  DCHECK(!decrypted_record_string.empty());

  base::StringPiece decrypted_record(decrypted_record_string);

  // Records must be at least two octets in size (to hold the padding). Records
  // that are smaller, i.e. a single octet, are invalid.
  if (decrypted_record.size() < sizeof(uint16_t))
    return false;

  // Records contain a two-byte, big-endian padding length followed by zero to
  // 65535 bytes of padding. Padding bytes must be zero but, since AES-GCM
  // authenticates the plaintext, checking and removing padding need not be done
  // in constant-time.
  uint16_t padding_length = (static_cast<uint8_t>(decrypted_record[0]) << 8) |
                            static_cast<uint8_t>(decrypted_record[1]);
  decrypted_record.remove_prefix(sizeof(uint16_t));

  if (padding_length > decrypted_record.size()) {
    return false;
  }

  for (size_t i = 0; i < padding_length; ++i) {
    if (decrypted_record[i] != 0)
      return false;
  }

  decrypted_record.remove_prefix(padding_length);
  decrypted_record.CopyToString(plaintext);
  return true;
}

std::string GCMMessageCryptographer::DerivePseudoRandomKey(
    const base::StringPiece& ikm) const {
  if (allow_empty_auth_secret_for_tests_ && auth_secret_.empty())
    return ikm.as_string();

  CHECK(!auth_secret_.empty());

  std::stringstream info_stream;
  info_stream << "Content-Encoding: auth" << '\x00';

  crypto::HKDF hkdf(ikm, auth_secret_,
                    info_stream.str(),
                    32, /* key_bytes_to_generate */
                    0,  /* iv_bytes_to_generate */
                    0   /* subkey_secret_bytes_to_generate */);

  return hkdf.client_write_key().as_string();
}

std::string GCMMessageCryptographer::DeriveContentEncryptionKey(
    const base::StringPiece& prk,
    const base::StringPiece& salt) const {
  crypto::HKDF hkdf(prk, salt,
                    content_encryption_key_info_,
                    kContentEncryptionKeySize,
                    0,  /* iv_bytes_to_generate */
                    0   /* subkey_secret_bytes_to_generate */);

  return hkdf.client_write_key().as_string();
}

std::string GCMMessageCryptographer::DeriveNonce(
    const base::StringPiece& prk,
    const base::StringPiece& salt) const {
  crypto::HKDF hkdf(prk, salt,
                    nonce_info_,
                    kNonceSize,
                    0,  /* iv_bytes_to_generate */
                    0   /* subkey_secret_bytes_to_generate */);

  // draft-thomson-http-encryption defines that the result should be XOR'ed with
  // the record's sequence number, however, Web Push encryption is limited to a
  // single record per draft-ietf-webpush-encryption.

  return hkdf.client_write_key().as_string();
}

}  // namespace gcm
