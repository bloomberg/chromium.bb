// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <openssl/evp.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/aes_key_openssl.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace content {

namespace webcrypto {

namespace {

const EVP_AEAD* GetAesKwAlgorithmFromKeySize(unsigned int key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_key_wrap();
    case 32:
      return EVP_aead_aes_256_key_wrap();
    default:
      return NULL;
  }
}

Status AesKwEncryptDecrypt(EncryptOrDecrypt mode,
                           const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           std::vector<uint8_t>* buffer) {
  // These length checks are done so the returned error matches that of NSS
  // implementation. Other than giving a more specific error, these are not
  // required.
  if ((mode == ENCRYPT && data.byte_length() < 16) ||
      (mode == DECRYPT && data.byte_length() < 24)) {
    return Status::ErrorDataTooSmall();
  }
  if (data.byte_length() % 8)
    return Status::ErrorInvalidAesKwDataLength();

  const std::vector<uint8_t>& raw_key =
      SymKeyOpenSsl::Cast(key)->raw_key_data();

  return AeadEncryptDecrypt(mode,
                            raw_key,
                            data,
                            8,             // tag_length_bytes
                            CryptoData(),  // iv
                            CryptoData(),  // additional_data
                            GetAesKwAlgorithmFromKeySize(raw_key.size()),
                            buffer);
}

class AesKwImplementation : public AesAlgorithm {
 public:
  AesKwImplementation()
      : AesAlgorithm(
            blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey,
            "KW") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesKwEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesKwEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesKwImplementation() {
  return new AesKwImplementation;
}

}  // namespace webcrypto

}  // namespace content
