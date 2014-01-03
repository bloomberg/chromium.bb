// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;
const size_t kAESNonceSize = 12;

void ClearOpenSslErrors() {
#ifdef NDEBUG
  while (ERR_get_error()) {}
#else
  while (long error = ERR_get_error()) {
    char buf[120];
    ERR_error_string_n(error, buf, arraysize(buf));
    DLOG(ERROR) << "OpenSSL error: " << buf;
  }
#endif
}

}  // namespace

Aes128Gcm12Encrypter::Aes128Gcm12Encrypter() {}

Aes128Gcm12Encrypter::~Aes128Gcm12Encrypter() {}

bool Aes128Gcm12Encrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());

  EVP_AEAD_CTX_cleanup(ctx_.get());

  if (!EVP_AEAD_CTX_init(ctx_.get(), EVP_aead_aes_128_gcm(), key_,
                         sizeof(key_), kAuthTagSize, NULL)) {
    ClearOpenSslErrors();
    return false;
  }

  return true;
}

bool Aes128Gcm12Encrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  COMPILE_ASSERT(sizeof(nonce_prefix_) == kNoncePrefixSize, bad_nonce_length);
  memcpy(nonce_prefix_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool Aes128Gcm12Encrypter::Encrypt(StringPiece nonce,
                                   StringPiece associated_data,
                                   StringPiece plaintext,
                                   unsigned char* output) {
  if (nonce.size() != kNoncePrefixSize + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }

  ssize_t len = EVP_AEAD_CTX_seal(
      ctx_.get(), output, plaintext.size() + kAuthTagSize,
      reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size(),
      reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size(),
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.size());

  if (len < 0) {
    ClearOpenSslErrors();
    return false;
  }

  return true;
}

QuicData* Aes128Gcm12Encrypter::EncryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece plaintext) {
  size_t ciphertext_size = GetCiphertextSize(plaintext.length());
  scoped_ptr<char[]> ciphertext(new char[ciphertext_size]);

  // TODO(ianswett): Introduce a check to ensure that we don't encrypt with the
  // same sequence number twice.
  uint8 nonce[kNoncePrefixSize + sizeof(sequence_number)];
  COMPILE_ASSERT(sizeof(nonce) == kAESNonceSize, bad_sequence_number_size);
  memcpy(nonce, nonce_prefix_, kNoncePrefixSize);
  memcpy(nonce + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  if (!Encrypt(StringPiece(reinterpret_cast<char*>(nonce), sizeof(nonce)),
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
  return StringPiece(reinterpret_cast<const char*>(nonce_prefix_),
                     kNoncePrefixSize);
}

}  // namespace net
