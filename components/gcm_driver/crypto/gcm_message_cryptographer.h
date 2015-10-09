// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_

#include <stdint.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"

namespace gcm {

// Messages delivered through GCM may be encrypted according to the IETF Web
// Push protocol, as described in draft-thomson-webpush-encryption-01:
//
// https://tools.ietf.org/html/draft-thomson-webpush-encryption-01
//
// This class implements the ability to encrypt or decrypt such messages using
// AEAD_AES_128_GCM with a 16-octet authentication tag. The encrypted payload
// will be stored in a single record as described in
// draft-thomson-http-encryption-01:
//
// https://tools.ietf.org/html/draft-thomson-http-encryption-01
//
// Note that while this class is not responsible for creating or storing the
// actual keys, it uses a key derivation function for the actual message
// encryption/decryption, thus allowing for the safe re-use of keys in multiple
// messages provided that a cryptographically-strong random salt is used.
class GCMMessageCryptographer {
 public:
  // Salt size, in bytes, that will be used together with the key to create a
  // unique content encryption key for a given message.
  static const size_t kSaltSize;

  GCMMessageCryptographer();
  ~GCMMessageCryptographer();

  // Encrypts |plaintext| using the |key| and the |salt|, both of which must be
  // 16 octets in length. The |plaintext| will be written to a single record,
  // and will include a 16 octet authentication tag. The encrypted result will
  // be written to |ciphertext|, the record size to |record_size|. This
  // implementation does not support prepending padding to the |plaintext|.
  bool Encrypt(const base::StringPiece& plaintext,
               const base::StringPiece& key,
               const base::StringPiece& salt,
               size_t* record_size,
               std::string* ciphertext) const WARN_UNUSED_RESULT;

  // Decrypts |ciphertext| using the |key| and the |salt|, both of which must be
  // 16 octets in length. The result will be stored in |plaintext|. Note that
  // there must only be a single record, per draft-thomson-http-encryption-01.
  bool Decrypt(const base::StringPiece& ciphertext,
               const base::StringPiece& key,
               const base::StringPiece& salt,
               size_t record_size,
               std::string* plaintext) const WARN_UNUSED_RESULT;

 private:
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, InvalidRecordPadding);
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, NonceGeneration);

  // Size, in bytes, of the authentication tag included in the messages.
  static const size_t kAuthenticationTagBytes;

  enum Mode { ENCRYPT, DECRYPT };

  // Private implementation of the encryption and decryption routines, provided
  // by either NSS or BoringSSL depending on the platform.
  bool EncryptDecryptRecordInternal(Mode mode,
                                    const base::StringPiece& input,
                                    const base::StringPiece& key,
                                    const base::StringPiece& nonce,
                                    std::string* output) const;

  // Derives the content encryption key from |key| and |salt|.
  std::string DeriveContentEncryptionKey(const base::StringPiece& key,
                                         const base::StringPiece& salt) const;

  // Derives the nonce from |key| and |salt|.
  std::string DeriveNonce(const base::StringPiece& key,
                          const base::StringPiece& salt) const;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
