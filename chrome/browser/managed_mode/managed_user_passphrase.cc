// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_passphrase.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

namespace {

// These constants are used as parameters when calling |DeriveKeyFromPassword|.
const int kNumberOfIterations = 1;
const int kDerivedKeySize = 128;
const int kSaltSize = 32;

} // namespace

ManagedUserPassphrase::ManagedUserPassphrase(const std::string& salt)
    : salt_(salt) {
  if (salt_.empty())
    GenerateRandomSalt();
}

ManagedUserPassphrase::~ManagedUserPassphrase() {
}

std::string ManagedUserPassphrase::GetSalt() {
  return salt_;
}

void ManagedUserPassphrase::GenerateRandomSalt() {
  std::string bytes;
  crypto::RandBytes(WriteInto(&bytes, kSaltSize+1), kSaltSize);
  bool success = base::Base64Encode(bytes, &salt_);
  DCHECK(success);
}

bool ManagedUserPassphrase::GenerateHashFromPassphrase(
    const std::string& passphrase,
    std::string* encoded_passphrase_hash) const {
  std::string passphrase_hash;
  if (!GetPassphraseHash(passphrase, &passphrase_hash))
    return false;
  return base::Base64Encode(passphrase_hash, encoded_passphrase_hash);
}

bool ManagedUserPassphrase::GetPassphraseHash(
    const std::string& passphrase,
    std::string* passphrase_hash) const {
  DCHECK(passphrase_hash);
  // Create a hash from the user-provided passphrase and the randomly generated
  // (or provided) salt.
  scoped_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::AES,
          passphrase,
          salt_,
          kNumberOfIterations,
          kDerivedKeySize));
  return encryption_key->GetRawKey(passphrase_hash);
}
