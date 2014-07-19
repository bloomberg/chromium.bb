// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_ALGORITHM_DISPATCH_H_
#define CONTENT_CHILD_WEBCRYPTO_ALGORITHM_DISPATCH_H_

#include <stdint.h>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class AlgorithmImplementation;
class CryptoData;
class Status;

// These functions provide an entry point for synchronous webcrypto operations.
//
// The inputs to these methods come from Blink, and hence the validations done
// by Blink can be assumed:
//
//   * The algorithm parameters are consistent with the algorithm
//   * The key contains the required usage for the operation

CONTENT_EXPORT Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                              const blink::WebCryptoKey& key,
                              const CryptoData& data,
                              std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                              const blink::WebCryptoKey& key,
                              const CryptoData& data,
                              std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                             const CryptoData& data,
                             std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status
    GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usage_mask,
                      blink::WebCryptoKey* key);

CONTENT_EXPORT Status
    GenerateKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* public_key,
                    blink::WebCryptoKey* private_key);

CONTENT_EXPORT Status ImportKey(blink::WebCryptoKeyFormat format,
                                const CryptoData& key_data,
                                const blink::WebCryptoAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usage_mask,
                                blink::WebCryptoKey* key);

CONTENT_EXPORT Status ExportKey(blink::WebCryptoKeyFormat format,
                                const blink::WebCryptoKey& key,
                                std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match);

CONTENT_EXPORT Status
    WrapKey(blink::WebCryptoKeyFormat format,
            const blink::WebCryptoKey& key_to_wrap,
            const blink::WebCryptoKey& wrapping_key,
            const blink::WebCryptoAlgorithm& wrapping_algorithm,
            std::vector<uint8_t>* buffer);

CONTENT_EXPORT Status
    UnwrapKey(blink::WebCryptoKeyFormat format,
              const CryptoData& wrapped_key_data,
              const blink::WebCryptoKey& wrapping_key,
              const blink::WebCryptoAlgorithm& wrapping_algorithm,
              const blink::WebCryptoAlgorithm& algorithm,
              bool extractable,
              blink::WebCryptoKeyUsageMask usage_mask,
              blink::WebCryptoKey* key);

CONTENT_EXPORT scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_ALGORITHM_DISPATCH_H_
