// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_KEY_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_KEY_OPENSSL_H_

#include "content/child/webcrypto/algorithm_implementation.h"

namespace content {

namespace webcrypto {

class PublicKeyNss;
class PrivateKeyNss;

// Base class for an RSA algorithm whose keys additionaly have a hash parameter
// bound to them. Provides functionality for generating, importing, and
// exporting keys.
class RsaHashedAlgorithm : public AlgorithmImplementation {
 public:
  // |all_public_key_usages| and |all_private_key_usages| are the set of
  // WebCrypto key usages that are valid for created keys (public and private
  // respectively).
  //
  // For instance if public keys support encryption and wrapping, and private
  // keys support decryption and unwrapping callers should set:
  //    all_public_key_usages = UsageEncrypt | UsageWrap
  //    all_private_key_usages = UsageDecrypt | UsageUnwrap
  // This information is used when importing or generating keys, to enforce
  // that valid key usages are allowed.
  RsaHashedAlgorithm(blink::WebCryptoKeyUsageMask all_public_key_usages,
                     blink::WebCryptoKeyUsageMask all_private_key_usages)
      : all_public_key_usages_(all_public_key_usages),
        all_private_key_usages_(all_private_key_usages) {}

  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE;

  virtual Status ImportKeyPkcs8(const CryptoData& key_data,
                                const blink::WebCryptoAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usage_mask,
                                blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status ImportKeySpki(const CryptoData& key_data,
                               const blink::WebCryptoAlgorithm& algorithm,
                               bool extractable,
                               blink::WebCryptoKeyUsageMask usage_mask,
                               blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                std::vector<uint8_t>* buffer) const OVERRIDE;

  virtual Status ExportKeySpki(const blink::WebCryptoKey& key,
                               std::vector<uint8_t>* buffer) const OVERRIDE;

 private:
  blink::WebCryptoKeyUsageMask all_public_key_usages_;
  blink::WebCryptoKeyUsageMask all_private_key_usages_;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_KEY_OPENSSL_H_
