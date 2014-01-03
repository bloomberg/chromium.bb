// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_12_decrypter.h"

#include <openssl/err.h>
#include <openssl/evp.h>

#include "base/memory/scoped_ptr.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kNoncePrefixSize = 4;
const size_t kAESNonceSize = 12;

}  // namespace

Aes128Gcm12Decrypter::Aes128Gcm12Decrypter() {}

Aes128Gcm12Decrypter::~Aes128Gcm12Decrypter() {}

// static
bool Aes128Gcm12Decrypter::IsSupported() { return true; }

bool Aes128Gcm12Decrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());

  EVP_AEAD_CTX_cleanup(ctx_.get());
  if (!EVP_AEAD_CTX_init(ctx_.get(), EVP_aead_aes_128_gcm(), key_,
                         sizeof(key_), kAuthTagSize, NULL)) {
    // Clear OpenSSL error stack.
    while (ERR_get_error()) {}
    return false;
  }

  return true;
}

bool Aes128Gcm12Decrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  COMPILE_ASSERT(sizeof(nonce_prefix_) == kNoncePrefixSize, bad_nonce_length);
  memcpy(nonce_prefix_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool Aes128Gcm12Decrypter::Decrypt(StringPiece nonce,
                                   StringPiece associated_data,
                                   StringPiece ciphertext,
                                   uint8* output,
                                   size_t* output_length) {
  if (ciphertext.length() < kAuthTagSize ||
      nonce.size() != kNoncePrefixSize + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }

  ssize_t len = EVP_AEAD_CTX_open(
      ctx_.get(), output, ciphertext.size(),
      reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size(),
      reinterpret_cast<const uint8_t*>(ciphertext.data()), ciphertext.size(),
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.size());

  if (len < 0) {
    // Clear OpenSSL error stack.
    while (ERR_get_error()) {}
    return false;
  }

  *output_length = len;
  return true;
}

QuicData* Aes128Gcm12Decrypter::DecryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece ciphertext) {
  if (ciphertext.length() < kAuthTagSize) {
    return NULL;
  }
  size_t plaintext_size = ciphertext.length();
  scoped_ptr<char[]> plaintext(new char[plaintext_size]);

  uint8 nonce[kNoncePrefixSize + sizeof(sequence_number)];
  COMPILE_ASSERT(sizeof(nonce) == kAESNonceSize, bad_sequence_number_size);
  memcpy(nonce, nonce_prefix_, kNoncePrefixSize);
  memcpy(nonce + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  if (!Decrypt(StringPiece(reinterpret_cast<char*>(nonce), sizeof(nonce)),
               associated_data, ciphertext,
               reinterpret_cast<uint8*>(plaintext.get()),
               &plaintext_size)) {
    return NULL;
  }
  return new QuicData(plaintext.release(), plaintext_size, true);
}

StringPiece Aes128Gcm12Decrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128Gcm12Decrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_prefix_),
                     kNoncePrefixSize);
}

}  // namespace net
