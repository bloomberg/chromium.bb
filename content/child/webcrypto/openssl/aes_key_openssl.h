// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_AES_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_AES_OPENSSL_H_

#include "content/child/webcrypto/algorithm_implementation.h"

namespace content {

namespace webcrypto {

// Base class for AES algorithms that provides the implementation for key
// creation and export.
class AesAlgorithm : public AlgorithmImplementation {
 public:
  // |all_key_usages| is the set of all WebCrypto key usages that are
  // allowed for imported or generated keys. |jwk_suffix| is the suffix
  // used when constructing JWK names for the algorithm. For instance A128CBC
  // is the JWK name for 128-bit AES-CBC. The |jwk_suffix| in this case would
  // be "CBC".
  AesAlgorithm(blink::WebCryptoKeyUsageMask all_key_usages,
               const std::string& jwk_suffix);

  // This is the same as the other AesAlgorithm constructor where
  // |all_key_usages| is pre-filled to values for encryption/decryption
  // algorithms (supports usages for: encrypt, decrypt, wrap, unwrap).
  explicit AesAlgorithm(const std::string& jwk_suffix);

  virtual Status VerifyKeyUsagesBeforeGenerateKey(
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE;

  virtual Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                                   bool extractable,
                                   blink::WebCryptoKeyUsageMask usage_mask,
                                   blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE;

  virtual Status ImportKeyRaw(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status ImportKeyJwk(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status ExportKeyRaw(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE;

  virtual Status ExportKeyJwk(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE;

 private:
  const blink::WebCryptoKeyUsageMask all_key_usages_;
  const std::string jwk_suffix_;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_AES_OPENSSL_H_
