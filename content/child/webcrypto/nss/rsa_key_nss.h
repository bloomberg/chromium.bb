// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_NSS_RSA_KEY_NSS_H_
#define CONTENT_CHILD_WEBCRYPTO_NSS_RSA_KEY_NSS_H_

#include <pkcs11t.h>

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
  // Constructs an RSA algorithm which will use the NSS flags |generate_flags|
  // when generating keys. |all_public_key_usages| and |all_private_key_usages|
  // are the set of WebCrypto key usages that are valid for created keys
  // (public and private respectively).
  //
  // For instance if public keys support encryption and wrapping, and private
  // keys support decryption and unwrapping callers should set:
  //    all_public_key_usages = UsageEncrypt | UsageWrap
  //    all_private_key_usages = UsageDecrypt | UsageUnwrap
  // This information is used when importing or generating keys, to enforce
  // that valid key usages are allowed.
  RsaHashedAlgorithm(CK_FLAGS generate_flags,
                     blink::WebCryptoKeyUsageMask all_public_key_usages,
                     blink::WebCryptoKeyUsageMask all_private_key_usages)
      : generate_flags_(generate_flags),
        all_public_key_usages_(all_public_key_usages),
        all_private_key_usages_(all_private_key_usages) {}

  // For instance "RSA-OAEP-256".
  virtual const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const = 0;

  virtual Status VerifyKeyUsagesBeforeGenerateKeyPair(
      blink::WebCryptoKeyUsageMask combined_usage_mask,
      blink::WebCryptoKeyUsageMask* public_usage_mask,
      blink::WebCryptoKeyUsageMask* private_usage_mask) const OVERRIDE;

  virtual Status GenerateKeyPair(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask public_usage_mask,
      blink::WebCryptoKeyUsageMask private_usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) const OVERRIDE;

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

  virtual Status ImportKeyJwk(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const OVERRIDE;

  virtual Status ExportKeyJwk(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE;

 private:
  CK_FLAGS generate_flags_;
  blink::WebCryptoKeyUsageMask all_public_key_usages_;
  blink::WebCryptoKeyUsageMask all_private_key_usages_;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_NSS_RSA_KEY_NSS_H_
