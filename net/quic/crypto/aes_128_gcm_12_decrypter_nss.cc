// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_12_decrypter.h"

#include <pk11pub.h>
#include <secerr.h>

#include "crypto/aes_128_gcm_helpers_nss.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;

SECStatus My_Decrypt(PK11SymKey* key,
                     CK_MECHANISM_TYPE mechanism,
                     SECItem* param,
                     unsigned char* out,
                     unsigned int* out_len,
                     unsigned int max_len,
                     const unsigned char* data,
                     unsigned int data_len) {
  return crypto::PK11DecryptHelper(key, mechanism, param, out, out_len, max_len,
                                   data, data_len);
}

}  // namespace

Aes128Gcm12Decrypter::Aes128Gcm12Decrypter()
    : AeadBaseDecrypter(CKM_AES_GCM, My_Decrypt, kKeySize, kAuthTagSize,
                        kNoncePrefixSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNoncePrefixSize <= kMaxNoncePrefixSize,
                "nonce prefix size too big");
}

Aes128Gcm12Decrypter::~Aes128Gcm12Decrypter() {}

void Aes128Gcm12Decrypter::FillAeadParams(StringPiece nonce,
                                          const StringPiece& associated_data,
                                          size_t auth_tag_size,
                                          AeadParams* aead_params) const {
  aead_params->len = sizeof(aead_params->data.gcm_params);
  CK_GCM_PARAMS* gcm_params = &aead_params->data.gcm_params;
  gcm_params->pIv =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(nonce.data()));
  gcm_params->ulIvLen = nonce.size();
  gcm_params->pAAD =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(associated_data.data()));
  gcm_params->ulAADLen = associated_data.size();
  gcm_params->ulTagBits = auth_tag_size * 8;
}

}  // namespace net
