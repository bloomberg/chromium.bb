// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_WEBCRYPTO_UTIL_H_
#define COMPONENTS_WEBCRYPTO_WEBCRYPTO_UTIL_H_

#include <stdint.h>
#include <string>

#include "base/values.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

// TODO(eroman): The remaining functions in this file need to be sorted
// out. They should be moved to a more domain specific location (for
// instance test helpers, asymmetric algorithm helpers, etc.).

namespace webcrypto {

class Status;

// Creates a WebCryptoAlgorithm without any parameters.
blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id);

// Creates an import algorithm for RSA algorithms that take a hash.
// It is an error to call this with a hash_id that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id);

// Creates an import algorithm for EC keys.
blink::WebCryptoAlgorithm CreateEcImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoNamedCurve named_curve);

// Returns true if the set bits in b make up a subset of the set bits in a.
bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b);

Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages,
                              bool allow_empty_usages);

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

// Verifies that |usages| is valid when importing a key of the given format.
Status VerifyUsagesBeforeImportAsymmetricKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask all_public_key_usages,
    blink::WebCryptoKeyUsageMask all_private_key_usages,
    blink::WebCryptoKeyUsageMask usages);

// Truncates an octet string to a particular bit length. This is accomplished by
// resizing to the closest byte length, and then zero-ing the unused
// least-significant bits of the final byte.
//
// It is an error to call this function with a bit length that is larger than
// that of |bytes|.
//
// TODO(eroman): This operation is not yet defined by the WebCrypto spec,
// however this is a reasonable interpretation:
// https://www.w3.org/Bugs/Public/show_bug.cgi?id=27402
void TruncateToBitLength(size_t length_bits, std::vector<uint8_t>* bytes);

// Rounds a bit count (up) to the nearest byte count.
//
// This is mathematically equivalent to (x + 7) / 8, however has no
// possibility of integer overflow.
template <typename T>
T NumBitsToBytes(T x) {
  return (x / 8) + (7 + (x % 8)) / 8;
}


// Splits the combined usages given to GenerateKey() into the respective usages
// for the public key and private key. Returns an error if the usages are
// invalid.
Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_WEBCRYPTO_UTIL_H_
