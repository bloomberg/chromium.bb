// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

#include <CommonCrypto/CommonCryptor.h>  // for kCCBlockSizeAES128

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "chrome/browser/password_manager/encryptor_password_mac.h"
#include "chrome/browser/keychain_mac.h"

namespace {

// Salt for Symmetric key derivation.
const char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
const size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
const size_t kEncryptionIterations = 1003;

// TODO(dhollowa): Refactor to allow dependency injection of Keychain.
static bool use_mock_keychain = false;

// Prefix for cypher text returned by current encryption version.  We prefix
// the cypher text with this string so that future data migration can detect
// this and migrate to different encryption without data loss.
const char kEncryptionVersionPrefix[] = "v10";

// Generates a newly allocated SymmetricKey object based on the password found
// in the Keychain.  The generated key is for AES encryption.  Ownership of the
// key is passed to the caller.  Returns NULL key in the case password access
// is denied or key generation error occurs.
crypto::SymmetricKey* GetEncryptionKey() {

  std::string password;
  if (use_mock_keychain) {
    password = "mock_password";
  } else {
    MacKeychain keychain;
    EncryptorPassword encryptor_password(keychain);
    password = encryptor_password.GetEncryptorPassword();
  }

  if (password.empty())
    return NULL;

  std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  scoped_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPassword(crypto::SymmetricKey::AES,
                                                  password,
                                                  salt,
                                                  kEncryptionIterations,
                                                  kDerivedKeySizeInBits));
  DCHECK(encryption_key.get());

  return encryption_key.release();
}

}  // namespace

bool Encryptor::EncryptString16(const string16& plaintext,
                                std::string* ciphertext) {
  return EncryptString(UTF16ToUTF8(plaintext), ciphertext);
}

bool Encryptor::DecryptString16(const std::string& ciphertext,
                                string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = UTF8ToUTF16(utf8);
  return true;
}

bool Encryptor::EncryptString(const std::string& plaintext,
                              std::string* ciphertext) {
  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  scoped_ptr<crypto::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cypher text with version information.
  ciphertext->insert(0, kEncryptionVersionPrefix);
  return true;
}

bool Encryptor::DecryptString(const std::string& ciphertext,
                              std::string* plaintext) {
  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  if (ciphertext.find(kEncryptionVersionPrefix) != 0) {
    *plaintext = ciphertext;
    return true;
  }

  // Strip off the versioning prefix before decrypting.
  std::string raw_ciphertext =
      ciphertext.substr(strlen(kEncryptionVersionPrefix));

  scoped_ptr<crypto::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(raw_ciphertext, plaintext))
    return false;

  return true;
}

void Encryptor::UseMockKeychain(bool use_mock) {
  use_mock_keychain = use_mock;
}

