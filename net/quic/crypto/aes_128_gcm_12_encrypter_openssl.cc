// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"

#include <openssl/evp.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/scoped_evp_cipher_ctx.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;

}  // namespace

Aes128Gcm12Encrypter::Aes128Gcm12Encrypter() {
}

// static
bool Aes128Gcm12Encrypter::IsSupported() { return true; }

bool Aes128Gcm12Encrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());
  return true;
}

bool Aes128Gcm12Encrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  memcpy(nonce_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool Aes128Gcm12Encrypter::Encrypt(StringPiece nonce,
                                   StringPiece associated_data,
                                   StringPiece plaintext,
                                   unsigned char* output) {
  // |output_len| is passed to an OpenSSL function to receive the output
  // length.
  int output_len;

  if (nonce.size() != kNoncePrefixSize + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }

  ScopedEVPCipherCtx ctx;

  // Set the cipher type and the key. The IV (nonce) is set below.
  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_gcm(), NULL, key_, NULL) == 0) {
    return false;
  }

  // Set the IV (nonce) length.
  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(),
                          NULL) == 0) {
    return false;
  }
  // Set the IV (nonce).
  if (EVP_EncryptInit_ex(
          ctx.get(), NULL, NULL, NULL,
          reinterpret_cast<const unsigned char*>(nonce.data())) == 0) {
    return false;
  }

  // If we pass a NULL, zero-length associated data to OpenSSL then it breaks.
  // Thus we only set non-empty associated data.
  if (!associated_data.empty()) {
    // Set the associated data. The second argument (output buffer) must be
    // NULL.
    if (EVP_EncryptUpdate(
            ctx.get(), NULL, &output_len,
            reinterpret_cast<const unsigned char*>(associated_data.data()),
            associated_data.size()) == 0) {
      return false;
    }
  }

  if (EVP_EncryptUpdate(
          ctx.get(), output, &output_len,
          reinterpret_cast<const unsigned char*>(plaintext.data()),
          plaintext.size()) == 0) {
    return false;
  }
  output += output_len;

  if (EVP_EncryptFinal_ex(ctx.get(), output, &output_len) == 0) {
    return false;
  }
  output += output_len;

  if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kAuthTagSize,
                          output) == 0) {
    return false;
  }

  return true;
}

QuicData* Aes128Gcm12Encrypter::EncryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece plaintext) {
  COMPILE_ASSERT(sizeof(nonce_) == kNoncePrefixSize + sizeof(sequence_number),
                 incorrect_nonce_size);
  memcpy(nonce_ + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));

  size_t ciphertext_size = GetCiphertextSize(plaintext.length());
  scoped_ptr<char[]> ciphertext(new char[ciphertext_size]);

  if (!Encrypt(StringPiece(reinterpret_cast<char*>(nonce_), sizeof(nonce_)),
               associated_data, plaintext,
               reinterpret_cast<unsigned char*>(ciphertext.get()))) {
    return NULL;
  }

  return new QuicData(ciphertext.release(), ciphertext_size, true);
}

size_t Aes128Gcm12Encrypter::GetKeySize() const { return kKeySize; }

size_t Aes128Gcm12Encrypter::GetNoncePrefixSize() const {
  return kNoncePrefixSize;
}

size_t Aes128Gcm12Encrypter::GetMaxPlaintextSize(size_t ciphertext_size) const {
  return ciphertext_size - kAuthTagSize;
}

// An AEAD_AES_128_GCM_12 ciphertext is exactly 12 bytes longer than its
// corresponding plaintext.
size_t Aes128Gcm12Encrypter::GetCiphertextSize(size_t plaintext_size) const {
  return plaintext_size + kAuthTagSize;
}

StringPiece Aes128Gcm12Encrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128Gcm12Encrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_), kNoncePrefixSize);
}

}  // namespace net
