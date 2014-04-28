// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_secret_boxer.h"

#include "base/logging.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_random.h"

using base::StringPiece;
using std::string;

namespace net {

// Defined kKeySize for GetKeySize() and SetKey().
static const size_t kKeySize = 16;

// kBoxNonceSize contains the number of bytes of nonce that we use in each box.
// TODO(rtenneti): Add support for kBoxNonceSize to be 16 bytes.
//
// From agl@:
//   96-bit nonces are on the edge. An attacker who can collect 2^41
//   source-address tokens has a 1% chance of finding a duplicate.
//
//   The "average" DDoS is now 32.4M PPS. That's 2^25 source-address tokens
//   per second. So one day of that DDoS botnot would reach the 1% mark.
//
//   It's not terrible, but it's not a "forget about it" margin.
static const size_t kBoxNonceSize = 12;

CryptoSecretBoxer::CryptoSecretBoxer()
    : encrypter_(QuicEncrypter::Create(kAESG)),
      decrypter_(QuicDecrypter::Create(kAESG)) {
}

CryptoSecretBoxer::~CryptoSecretBoxer() {}

// static
size_t CryptoSecretBoxer::GetKeySize() { return kKeySize; }

bool CryptoSecretBoxer::SetKey(StringPiece key) {
  DCHECK_EQ(static_cast<size_t>(kKeySize), key.size());
  string key_string = key.as_string();
  if (!encrypter_->SetKey(key_string)) {
    DLOG(DFATAL) << "CryptoSecretBoxer's encrypter_->SetKey failed.";
    return false;
  }
  if (!decrypter_->SetKey(key_string)) {
    DLOG(DFATAL) << "CryptoSecretBoxer's decrypter_->SetKey failed.";
    return false;
  }
  return true;
}

string CryptoSecretBoxer::Box(QuicRandom* rand, StringPiece plaintext) const {
  DCHECK_EQ(kKeySize, encrypter_->GetKeySize());
  size_t ciphertext_size = encrypter_->GetCiphertextSize(plaintext.length());

  string ret;
  const size_t len = kBoxNonceSize + ciphertext_size;
  ret.resize(len);
  char* data = &ret[0];

  // Generate nonce.
  rand->RandBytes(data, kBoxNonceSize);
  memcpy(data + kBoxNonceSize, plaintext.data(), plaintext.size());

  if (!encrypter_->Encrypt(StringPiece(data, kBoxNonceSize), StringPiece(),
                           plaintext, reinterpret_cast<unsigned char*>(
                                          data + kBoxNonceSize))) {
    DLOG(DFATAL) << "CryptoSecretBoxer's Encrypt failed.";
    return string();
  }

  return ret;
}

bool CryptoSecretBoxer::Unbox(StringPiece ciphertext,
                              string* out_storage,
                              StringPiece* out) const {
  if (ciphertext.size() < kBoxNonceSize) {
    return false;
  }

  char nonce[kBoxNonceSize];
  memcpy(nonce, ciphertext.data(), kBoxNonceSize);
  ciphertext.remove_prefix(kBoxNonceSize);

  size_t len = ciphertext.size();
  out_storage->resize(len);
  char* data = const_cast<char*>(out_storage->data());

  if (!decrypter_->Decrypt(StringPiece(nonce, kBoxNonceSize), StringPiece(),
                           ciphertext, reinterpret_cast<unsigned char*>(data),
                           &len)) {
    return false;
  }

  out->set(data, len);
  return true;
}

}  // namespace net
