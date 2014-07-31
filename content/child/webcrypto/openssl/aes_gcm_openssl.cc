// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <openssl/evp.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/aes_key_openssl.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

namespace {

const EVP_AEAD* GetAesGcmAlgorithmFromKeySize(unsigned int key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_gcm();
    case 32:
      return EVP_aead_aes_256_gcm();
    default:
      return NULL;
  }
}

Status AesGcmEncryptDecrypt(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  const std::vector<uint8_t>& raw_key =
      SymKeyOpenSsl::Cast(key)->raw_key_data();
  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();

  unsigned int tag_length_bits;
  Status status = GetAesGcmTagLengthInBits(params, &tag_length_bits);
  if (status.IsError())
    return status;

  return AeadEncryptDecrypt(mode,
                            raw_key,
                            data,
                            tag_length_bits / 8,
                            CryptoData(params->iv()),
                            CryptoData(params->optionalAdditionalData()),
                            GetAesGcmAlgorithmFromKeySize(raw_key.size()),
                            buffer);
}

class AesGcmImplementation : public AesAlgorithm {
 public:
  AesGcmImplementation() : AesAlgorithm("GCM") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesGcmImplementation() {
  return new AesGcmImplementation;
}

}  // namespace webcrypto

}  // namespace content
