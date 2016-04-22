// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"

namespace gcm {

// Messages delivered through GCM may be encrypted according to the IETF Web
// Push protocol, as described in draft-ietf-webpush-encryption:
//
// https://tools.ietf.org/html/draft-ietf-webpush-encryption
//
// This class implements the ability to encrypt or decrypt such messages using
// AEAD_AES_128_GCM with a 16-octet authentication tag. The encrypted payload
// will be stored in a single record as described in
// draft-thomson-http-encryption:
//
// https://tools.ietf.org/html/draft-thomson-http-encryption
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

  // Label of the encryption group used to calculate the shared secret.
  enum class Label {
    P256
  };

  // Creates a new cryptographer with |label|, identifying the group used for
  // the key agreement, and the public keys of both the recipient and sender.
  GCMMessageCryptographer(Label label,
                          const base::StringPiece& recipient_public_key,
                          const base::StringPiece& sender_public_key,
                          const std::string& auth_secret);

  ~GCMMessageCryptographer();

  // Encrypts |plaintext| using the |ikm| and the |salt|, both of which must be
  // 16 octets in length. The |plaintext| will be written to a single record,
  // and will include a 16 octet authentication tag. The encrypted result will
  // be written to |ciphertext|, the record size to |record_size|. This
  // implementation does not support prepending padding to the |plaintext|.
  bool Encrypt(const base::StringPiece& plaintext,
               const base::StringPiece& ikm,
               const base::StringPiece& salt,
               size_t* record_size,
               std::string* ciphertext) const WARN_UNUSED_RESULT;

  // Decrypts |ciphertext| using the |ikm| and the |salt|, both of which must be
  // 16 octets in length. The result will be stored in |plaintext|. Note that
  // there must only be a single record, per draft-thomson-http-encryption-01.
  bool Decrypt(const base::StringPiece& ciphertext,
               const base::StringPiece& ikm,
               const base::StringPiece& salt,
               size_t record_size,
               std::string* plaintext) const WARN_UNUSED_RESULT;

 private:
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, AuthSecretAffectsIKM);
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, InvalidRecordPadding);
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, NonceGeneration);
  friend class GCMMessageCryptographerReferenceTest;

  // Size, in bytes, of the authentication tag included in the messages.
  static const size_t kAuthenticationTagBytes;

  enum Mode { ENCRYPT, DECRYPT };

  // Private implementation of the encryption and decryption routines, provided
  // by BoringSSL.
  bool EncryptDecryptRecordInternal(Mode mode,
                                    const base::StringPiece& input,
                                    const base::StringPiece& key,
                                    const base::StringPiece& nonce,
                                    std::string* output) const;

  // Derives the pseuro random key (PRK) to use for deriving the content
  // encryption key and the nonce. If |auth_secret_| is not the empty string,
  // another HKDF will be invoked between the |key| and the |auth_secret_|.
  std::string DerivePseudoRandomKey(const base::StringPiece& ikm) const;

  // Derives the content encryption key from |prk| and |salt|.
  std::string DeriveContentEncryptionKey(const base::StringPiece& prk,
                                         const base::StringPiece& salt) const;

  // Derives the nonce from |prk| and |salt|.
  std::string DeriveNonce(const base::StringPiece& prk,
                          const base::StringPiece& salt) const;

  // The info parameters to the HKDFs used for deriving the content encryption
  // key and the nonce. These contain the label of the used curve, as well as
  // the sender and recipient's public keys.
  std::string content_encryption_key_info_;
  std::string nonce_info_;

  // The pre-shared authentication secret associated with the subscription.
  std::string auth_secret_;

  // Whether an empty auth secret is acceptable when deriving the IKM. This only
  // is the case when running tests against the reference vectors.
  bool allow_empty_auth_secret_for_tests_ = false;

  void set_allow_empty_auth_secret_for_tests(bool value) {
    allow_empty_auth_secret_for_tests_ = value;
  }
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
