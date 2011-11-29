// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encryptor.h"

#include <string.h>

#include "base/string_util.h"
#include "crypto/symmetric_key.h"

namespace crypto {

namespace {

// On success, returns the block size (in bytes) for the algorithm that |key|
// is for. On failure, returns 0.
DWORD GetCipherBlockSize(HCRYPTKEY key) {
  DWORD block_size_in_bits = 0;
  DWORD param_size = sizeof(block_size_in_bits);
  BOOL ok = CryptGetKeyParam(key, KP_BLOCKLEN,
                             reinterpret_cast<BYTE*>(&block_size_in_bits),
                             &param_size, 0);
  if (!ok)
    return 0;

  return block_size_in_bits / 8;
}

}  // namespace

Encryptor::Encryptor()
    : key_(NULL),
      mode_(CBC),
      block_size_(0) {
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key,
                     Mode mode,
                     const base::StringPiece& iv) {
  DCHECK(key);
  DCHECK_EQ(CBC, mode) << "Unsupported mode of operation";

  // In CryptoAPI, the IV, padding mode, and feedback register (for a chaining
  // mode) are properties of a key, so we have to create a copy of the key for
  // the Encryptor.  See the Remarks section of the CryptEncrypt MSDN page.
  BOOL ok = CryptDuplicateKey(key->key(), NULL, 0, capi_key_.receive());
  if (!ok)
    return false;

  // CRYPT_MODE_CBC is the default for Microsoft Base Cryptographic Provider,
  // but we set it anyway to be safe.
  DWORD cipher_mode = CRYPT_MODE_CBC;
  ok = CryptSetKeyParam(capi_key_.get(), KP_MODE,
                        reinterpret_cast<BYTE*>(&cipher_mode), 0);
  if (!ok)
    return false;

  block_size_ = GetCipherBlockSize(capi_key_.get());
  if (block_size_ == 0)
    return false;

  if (iv.size() != block_size_)
    return false;

  ok = CryptSetKeyParam(capi_key_.get(), KP_IV,
                        reinterpret_cast<const BYTE*>(iv.data()), 0);
  if (!ok)
    return false;

  DWORD padding_method = PKCS5_PADDING;
  ok = CryptSetKeyParam(capi_key_.get(), KP_PADDING,
                        reinterpret_cast<BYTE*>(&padding_method), 0);
  if (!ok)
    return false;

  return true;
}

bool Encryptor::Encrypt(const base::StringPiece& plaintext,
                        std::string* ciphertext) {
  DWORD data_len = plaintext.size();
  CHECK((data_len > 0u) || (mode_ == CBC));
  DWORD total_len = data_len + block_size_;
  CHECK_GT(total_len, 0u);
  CHECK_GT(total_len + 1, data_len);

  // CryptoAPI encrypts/decrypts in place.
  char* ciphertext_data = WriteInto(ciphertext, total_len + 1);
  memcpy(ciphertext_data, plaintext.data(), data_len);

  BOOL ok = CryptEncrypt(capi_key_.get(), NULL, TRUE, 0,
                         reinterpret_cast<BYTE*>(ciphertext_data), &data_len,
                         total_len);
  if (!ok) {
    ciphertext->clear();
    return false;
  }

  ciphertext->resize(data_len);
  return true;
}

bool Encryptor::Decrypt(const base::StringPiece& ciphertext,
                        std::string* plaintext) {
  DWORD data_len = ciphertext.size();
  CHECK_GT(data_len, 0u);
  CHECK_GT(data_len + 1, data_len);

  // CryptoAPI encrypts/decrypts in place.
  char* plaintext_data = WriteInto(plaintext, data_len + 1);
  memcpy(plaintext_data, ciphertext.data(), data_len);

  BOOL ok = CryptDecrypt(capi_key_.get(), NULL, TRUE, 0,
                         reinterpret_cast<BYTE*>(plaintext_data), &data_len);
  if (!ok) {
    plaintext->clear();
    return false;
  }

  plaintext->resize(data_len);
  return true;
}

}  // namespace crypto
