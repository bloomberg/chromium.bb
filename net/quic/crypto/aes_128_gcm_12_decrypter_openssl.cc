// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_12_decrypter.h"

#include <openssl/evp.h>

#include "base/memory/scoped_ptr.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
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

  // Set the cipher type and the key.
  if (EVP_EncryptInit_ex(ctx_.get(), EVP_aes_128_gcm(), NULL, key_,
                         NULL) == 0) {
    return false;
  }

  // Set the IV (nonce) length.
  if (EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_GCM_SET_IVLEN, kAESNonceSize,
                          NULL) == 0) {
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
  const size_t plaintext_size = ciphertext.length() - kAuthTagSize;

  // Set the IV (nonce).
  if (EVP_DecryptInit_ex(
          ctx_.get(), NULL, NULL, NULL,
          reinterpret_cast<const uint8*>(nonce.data())) == 0) {
    return false;
  }

  // Set the authentication tag.
  if (EVP_CIPHER_CTX_ctrl(
          ctx_.get(), EVP_CTRL_GCM_SET_TAG, kAuthTagSize,
          const_cast<char*>(ciphertext.data()) + plaintext_size) == 0) {
    return false;
  }

  // If we pass a NULL, zero-length associated data to OpenSSL then it breaks.
  // Thus we only set non-empty associated data.
  if (!associated_data.empty()) {
    // Set the associated data. The second argument (output buffer) must be
    // NULL.
    int unused_len;
    if (EVP_DecryptUpdate(
            ctx_.get(), NULL, &unused_len,
            reinterpret_cast<const uint8*>(associated_data.data()),
            associated_data.size()) == 0) {
      return false;
    }
  }

  int len;
  if (EVP_DecryptUpdate(
          ctx_.get(), output, &len,
          reinterpret_cast<const uint8*>(ciphertext.data()),
          plaintext_size) == 0) {
    return false;
  }
  output += len;

  if (EVP_DecryptFinal_ex(ctx_.get(), output, &len) == 0) {
    return false;
  }
  output += len;

  *output_length = plaintext_size;

  return true;
}

QuicData* Aes128Gcm12Decrypter::DecryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece ciphertext) {
  if (ciphertext.length() < kAuthTagSize) {
    return NULL;
  }
  size_t plaintext_size;
  scoped_ptr<char[]> plaintext(new char[ciphertext.length()]);

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
