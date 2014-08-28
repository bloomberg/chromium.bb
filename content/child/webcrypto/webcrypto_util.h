// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_
#define CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_

#include <stdint.h>
#include <string>

#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

class Status;

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
CONTENT_EXPORT Status GetWebCryptoUsagesFromJwkKeyOps(
    const base::ListValue* jwk_key_ops_value,
    blink::WebCryptoKeyUsageMask* jwk_key_ops_mask);

// Composes a JWK key_ops array from a Web Crypto usage mask.
base::ListValue* CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usage_mask);

// Creates a WebCryptoAlgorithm without any parameters.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateAlgorithm(
    blink::WebCryptoAlgorithmId id);

// Creates an HMAC import algorithm whose inner hash algorithm is determined by
// the specified algorithm ID. It is an error to call this method with a hash
// algorithm that is not SHA*.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id);

// Creates an import algorithm for RSA algorithms that take a hash.
// It is an error to call this with a hash_id that is not a SHA*.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id);

// Returns true if the set bits in b make up a subset of the set bits in a.
bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b);

bool KeyUsageAllows(const blink::WebCryptoKey& key,
                    const blink::WebCryptoKeyUsage usage);

bool IsAlgorithmRsa(blink::WebCryptoAlgorithmId alg_id);
bool IsAlgorithmAsymmetric(blink::WebCryptoAlgorithmId alg_id);

Status GetAesGcmTagLengthInBits(const blink::WebCryptoAesGcmParams* params,
                                unsigned int* tag_length_bits);

Status GetAesKeyGenLengthInBits(const blink::WebCryptoAesKeyGenParams* params,
                                unsigned int* keylen_bits);

Status GetHmacKeyGenLengthInBits(const blink::WebCryptoHmacKeyGenParams* params,
                                 unsigned int* keylen_bits);

Status VerifyAesKeyLengthForImport(unsigned int keylen_bytes);

Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages);

// Extracts the public exponent and modulus length from the Blink parameters.
// On success it is guaranteed that:
//   * public_exponent is either 3 or 65537
//   * modulus_length_bits is a multiple of 8
//   * modulus_length is >= 256
//   * modulus_length is <= 16K
Status GetRsaKeyGenParameters(
    const blink::WebCryptoRsaHashedKeyGenParams* params,
    unsigned int* public_exponent,
    unsigned int* modulus_length_bits);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_
