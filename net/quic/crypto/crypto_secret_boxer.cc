// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_secret_boxer.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/quic/crypto/quic_random.h"

using base::StringPiece;
using std::string;

namespace net {

// Defined kKeySize for GetKeySize() and SetKey().
static const size_t kKeySize = 16;

// kBoxNonceSize contains the number of bytes of nonce that we use in each box.
static const size_t kBoxNonceSize = 16;

// static
size_t CryptoSecretBoxer::GetKeySize() { return kKeySize; }

void CryptoSecretBoxer::SetKey(StringPiece key) {
  DCHECK_EQ(static_cast<size_t>(kKeySize), key.size());
  key_ = key.as_string();
}

// TODO(rtenneti): Delete sha256 based code. Use Aes128Gcm12Encrypter to Box the
// plaintext. This is temporary solution for tests to pass.
string CryptoSecretBoxer::Box(QuicRandom* rand, StringPiece plaintext) const {
  string ret;
  const size_t len = kBoxNonceSize + plaintext.size() + crypto::kSHA256Length;
  ret.resize(len);
  char* data = &ret[0];

  // Generate nonce.
  rand->RandBytes(data, kBoxNonceSize);
  memcpy(data + kBoxNonceSize, plaintext.data(), plaintext.size());

  // Compute sha256 for nonce + plaintext.
  scoped_ptr<crypto::SecureHash> sha256(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  sha256->Update(data, kBoxNonceSize + plaintext.size());
  sha256->Finish(data + kBoxNonceSize + plaintext.size(),
                 crypto::kSHA256Length);

  return ret;
}

// TODO(rtenneti): Delete sha256 based code. Use Aes128Gcm12Decrypter to Unbox
// the plaintext. This is temporary solution for tests to pass.
bool CryptoSecretBoxer::Unbox(StringPiece ciphertext,
                              string* out_storage,
                              StringPiece* out) const {
  if (ciphertext.size() < kBoxNonceSize + crypto::kSHA256Length) {
    return false;
  }

  const size_t plaintext_len =
      ciphertext.size() - kBoxNonceSize - crypto::kSHA256Length;
  out_storage->resize(plaintext_len);
  char* data = const_cast<char*>(out_storage->data());

  // Copy plaintext from ciphertext.
  if (plaintext_len != 0) {
    memcpy(data, ciphertext.data() + kBoxNonceSize, plaintext_len);
  }

  // Compute sha256 for nonce + plaintext.
  scoped_ptr<crypto::SecureHash> sha256(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  sha256->Update(ciphertext.data(), ciphertext.size() - crypto::kSHA256Length);
  char sha256_bytes[crypto::kSHA256Length];
  sha256->Finish(sha256_bytes, sizeof(sha256_bytes));

  // Verify sha256.
  if (0 != memcmp(ciphertext.data() + ciphertext.size() - crypto::kSHA256Length,
                  sha256_bytes, crypto::kSHA256Length)) {
    return false;
  }

  out->set(data, plaintext_len);
  return true;
}

}  // namespace net
