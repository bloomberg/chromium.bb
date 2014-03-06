// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_SHARED_CRYPTO_H_
#define CONTENT_RENDERER_WEBCRYPTO_SHARED_CRYPTO_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

class CryptoData;

class Status;

// Do one-time initialization. It is safe to call this multiple times.
CONTENT_EXPORT void Init();

// The functions exported by shared_crypto.h provide a common entry point for
// synchronous crypto operations.
//
// Here is how the layer cake looks.
//
//              Blink
//                |
//  ==============|==========================
//                |
//             content
//                |
//                |
//                v
//          WebCryptoImpl     (Implements the blink::WebCrypto interface for
//                |            asynchronous completions)
//                |
//                |      [shared_crypto_unittest.cc]
//                |           /
//                |          /   (The blink::WebCrypto interface is not
//                |         /     testable from the chromium side because
//                |        /      the result object is not mockable.
//                |       /       Tests are done on shared_crypto instead.
//                V      v
//        [shared_crypto.h]   (Exposes synchronous functions in the
//                |            webcrypto:: namespace. This does
//                |            common validations, infers default
//                |            parameters, and casts the algorithm
//                |            parameters to the right types)
//                |
//                V
//       [platform_crypto.h]  (Exposes functions in the webcrypto::platform
//                |            namespace)
//                |
//                |
//                V
//  [platform_crypto_{nss|openssl}.cc]  (Implements using the platform crypto
//                                       library)
//
// The shared_crypto.h functions are responsible for:
//
//  * Validating the key usages
//  * Inferring default parameters when not specified
//  * Validating key exportability
//  * Validating algorithm with key.algorithm
//  * Converting the blink key to a more specific platform::{PublicKey,
//    PrivateKey, SymKey} and making sure it was the right type.
//  * Validating alogorithm specific parameters (for instance, was the iv for
//    AES-CBC 16 bytes).
//  * Parse a JWK

CONTENT_EXPORT Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                              const blink::WebCryptoKey& key,
                              const CryptoData& data,
                              blink::WebArrayBuffer* buffer);

CONTENT_EXPORT Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                              const blink::WebCryptoKey& key,
                              const CryptoData& data,
                              blink::WebArrayBuffer* buffer);

CONTENT_EXPORT Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer);

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

CONTENT_EXPORT Status
    ImportKey(blink::WebCryptoKeyFormat format,
              const CryptoData& key_data,
              const blink::WebCryptoAlgorithm& algorithm_or_null,
              bool extractable,
              blink::WebCryptoKeyUsageMask usage_mask,
              blink::WebCryptoKey* key);

CONTENT_EXPORT Status ExportKey(blink::WebCryptoKeyFormat format,
                                const blink::WebCryptoKey& key,
                                blink::WebArrayBuffer* buffer);

CONTENT_EXPORT Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           blink::WebArrayBuffer* buffer);

CONTENT_EXPORT Status
    VerifySignature(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& key,
                    const CryptoData& signature,
                    const CryptoData& data,
                    bool* signature_match);

CONTENT_EXPORT Status
    ImportKeyJwk(const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm_or_null,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usage_mask,
                 blink::WebCryptoKey* key);

CONTENT_EXPORT Status
    WrapKey(blink::WebCryptoKeyFormat format,
            const blink::WebCryptoKey& wrapping_key,
            const blink::WebCryptoKey& key_to_wrap,
            const blink::WebCryptoAlgorithm& wrapping_algorithm,
            blink::WebArrayBuffer* buffer);

CONTENT_EXPORT Status
    UnwrapKey(blink::WebCryptoKeyFormat format,
              const CryptoData& wrapped_key_data,
              const blink::WebCryptoKey& wrapping_key,
              const blink::WebCryptoAlgorithm& wrapping_algorithm,
              const blink::WebCryptoAlgorithm& algorithm_or_null,
              bool extractable,
              blink::WebCryptoKeyUsageMask usage_mask,
              blink::WebCryptoKey* key);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_SHARED_CRYPTO_H_
