// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_CRYPTO_ALGORITHM_IMPLEMENTATION_H_
#define CONTENT_CHILD_WEBCRYPTO_CRYPTO_ALGORITHM_IMPLEMENTATION_H_

#include <stdint.h>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

// AlgorithmImplementation is a base class for *executing* the operations of an
// algorithm (generating keys, encrypting, signing, etc.).
//
// This is in contrast to blink::WebCryptoAlgorithm which instead *describes*
// the operation and its parameters.
//
// AlgorithmImplementation has reasonable default implementations for all
// methods which behave as if the operation is it is unsupported, so
// implementations need only override the applicable methods.
//
// Unless stated otherwise methods of AlgorithmImplementation are responsible
// for sanitizing their inputs. The following can be assumed:
//
//   * |algorithm.id()| and |key.algorithm.id()| matches the algorithm under
//     which the implementation was registerd.
//   * |algorithm| has the correct parameters type for the operation.
//   * The key usages have already been verified. In fact in the case of calls
//     to Encrypt()/Decrypt() the corresponding key usages may not be present
//     (when wrapping/unwrapping).
class AlgorithmImplementation {
 public:
  virtual ~AlgorithmImplementation();

  // This method corresponds to Web Crypto's crypto.subtle.encrypt().
  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const;

  // This method corresponds to Web Crypto's crypto.subtle.decrypt().
  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const;

  // This method corresponds to Web Crypto's crypto.subtle.sign().
  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) const;

  // This method corresponds to Web Crypto's crypto.subtle.verify().
  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        const CryptoData& signature,
                        const CryptoData& data,
                        bool* signature_match) const;

  // This method corresponds to Web Crypto's crypto.subtle.digest().
  virtual Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                        const CryptoData& data,
                        std::vector<uint8_t>* buffer) const;

  // VerifyKeyUsagesBeforeGenerateKey() must be called prior to
  // GenerateSecretKey() to validate the requested key usages.
  virtual Status VerifyKeyUsagesBeforeGenerateKey(
      blink::WebCryptoKeyUsageMask usage_mask) const;

  // This method corresponds to Web Crypto's crypto.subtle.generateKey().
  virtual Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                                   bool extractable,
                                   blink::WebCryptoKeyUsageMask usage_mask,
                                   blink::WebCryptoKey* key) const;

  // VerifyKeyUsagesBeforeGenerateKeyPair() must be called prior to
  // GenerateKeyPair() to validate the requested key usages.
  virtual Status VerifyKeyUsagesBeforeGenerateKeyPair(
      blink::WebCryptoKeyUsageMask combined_usage_mask,
      blink::WebCryptoKeyUsageMask* public_usage_mask,
      blink::WebCryptoKeyUsageMask* private_usage_mask) const;

  // This method corresponds to Web Crypto's crypto.subtle.generateKey().
  virtual Status GenerateKeyPair(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask public_usage_mask,
      blink::WebCryptoKeyUsageMask private_usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) const;

  // -----------------------------------------------
  // Key import
  // -----------------------------------------------

  // VerifyKeyUsagesBeforeImportKey() must be called before either
  // importing a key, or unwrapping a key.
  //
  // Implementations should return an error if the requested usages are invalid
  // when importing for the specified format.
  //
  // For instance, importing an RSA-SSA key with 'spki' format and Sign usage
  // is invalid. The 'spki' format implies it will be a public key, and public
  // keys do not support signing.
  //
  // When called with format=JWK the key type may be unknown. The
  // ImportKeyJwk() must do the final usage check.
  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const;

  // This method corresponds to Web Crypto's
  // crypto.subtle.importKey(format='raw').
  virtual Status ImportKeyRaw(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const;

  // This method corresponds to Web Crypto's
  // crypto.subtle.importKey(format='pkcs8').
  virtual Status ImportKeyPkcs8(const CryptoData& key_data,
                                const blink::WebCryptoAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usage_mask,
                                blink::WebCryptoKey* key) const;

  // This method corresponds to Web Crypto's
  // crypto.subtle.importKey(format='spki').
  virtual Status ImportKeySpki(const CryptoData& key_data,
                               const blink::WebCryptoAlgorithm& algorithm,
                               bool extractable,
                               blink::WebCryptoKeyUsageMask usage_mask,
                               blink::WebCryptoKey* key) const;

  // This method corresponds to Web Crypto's
  // crypto.subtle.importKey(format='jwk').
  virtual Status ImportKeyJwk(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const;

  // -----------------------------------------------
  // Key export
  // -----------------------------------------------

  virtual Status ExportKeyRaw(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const;

  virtual Status ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                std::vector<uint8_t>* buffer) const;

  virtual Status ExportKeySpki(const blink::WebCryptoKey& key,
                               std::vector<uint8_t>* buffer) const;

  virtual Status ExportKeyJwk(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_CRYPTO_ALGORITHM_IMPLEMENTATION_H_
