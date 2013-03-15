// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_decrypter.h"

#include "base/memory/scoped_ptr.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;
const size_t kAuthTagSize = 16;

}  // namespace

// static
bool Aes128GcmDecrypter::IsSupported() {
  return false;
}

bool Aes128GcmDecrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());
  return true;
}

bool Aes128GcmDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  memcpy(nonce_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

QuicData* Aes128GcmDecrypter::Decrypt(QuicPacketSequenceNumber sequence_number,
                                      StringPiece associated_data,
                                      StringPiece ciphertext) {
  COMPILE_ASSERT(sizeof(nonce_) == kNoncePrefixSize + sizeof(sequence_number),
                 incorrect_nonce_size);
  memcpy(nonce_ + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  return DecryptWithNonce(StringPiece(reinterpret_cast<char*>(nonce_),
                                      sizeof(nonce_)),
                          associated_data, ciphertext);
}

StringPiece Aes128GcmDecrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128GcmDecrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_), kNoncePrefixSize);
}

QuicData* Aes128GcmDecrypter::DecryptWithNonce(StringPiece nonce,
                                               StringPiece associated_data,
                                               StringPiece ciphertext) {
  if (ciphertext.length() < kAuthTagSize) {
    return NULL;
  }
  size_t plaintext_size = ciphertext.length() - kAuthTagSize;
  scoped_ptr<char[]> plaintext(new char[plaintext_size]);

  // TODO(wtc): implement this function using NSS.

  return new QuicData(plaintext.release(), plaintext_size, true);
}

}  // namespace net
