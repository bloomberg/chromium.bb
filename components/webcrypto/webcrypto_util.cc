// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/webcrypto_util.h"

#include "base/logging.h"
#include "components/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned int.
bool BigIntegerToUint(const uint8_t* data,
                      size_t data_size,
                      unsigned int* result) {
  if (data_size == 0)
    return false;

  *result = 0;
  for (size_t i = 0; i < data_size; ++i) {
    size_t reverse_i = data_size - i - 1;

    if (reverse_i >= sizeof(*result) && data[i])
      return false;  // Too large for a uint.

    *result |= data[i] << 8 * reverse_i;
  }
  return true;
}


}  // namespace

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id, new blink::WebCryptoRsaHashedImportParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateEcImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoNamedCurve named_curve) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id, new blink::WebCryptoEcKeyImportParams(named_curve));
}

bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b) {
  return (a & b) == b;
}


Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages,
                              bool allow_empty_usages) {
  if (!allow_empty_usages && actual_usages == 0)
    return Status::ErrorCreateKeyEmptyUsages();

  if (!ContainsKeyUsages(all_possible_usages, actual_usages))
    return Status::ErrorCreateKeyBadUsages();
  return Status::Success();
}

Status GetRsaKeyGenParameters(
    const blink::WebCryptoRsaHashedKeyGenParams* params,
    unsigned int* public_exponent,
    unsigned int* modulus_length_bits) {
  *modulus_length_bits = params->modulusLengthBits();

  // Limit the RSA key sizes to:
  //   * Multiple of 8 bits
  //   * 256 bits to 16K bits
  // This corresponds to the values that NSS would allow, which was relevant
  // back when Chromium's WebCrypto supported both OpenSSL and NSS.
  if (*modulus_length_bits < 256 || *modulus_length_bits > 16384 ||
      (*modulus_length_bits % 8) != 0) {
    return Status::ErrorGenerateRsaUnsupportedModulus();
  }

  if (!BigIntegerToUint(params->publicExponent().data(),
                        params->publicExponent().size(), public_exponent)) {
    return Status::ErrorGenerateKeyPublicExponent();
  }

  // OpenSSL hangs when given bad public exponents. Use a whitelist to avoid
  // feeding OpenSSL data that could hang.
  if (*public_exponent != 3 && *public_exponent != 65537)
    return Status::ErrorGenerateKeyPublicExponent();

  return Status::Success();
}

Status VerifyUsagesBeforeImportAsymmetricKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask all_public_key_usages,
    blink::WebCryptoKeyUsageMask all_private_key_usages,
    blink::WebCryptoKeyUsageMask usages) {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckKeyCreationUsages(all_public_key_usages, usages, true);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckKeyCreationUsages(all_private_key_usages, usages, false);
    case blink::WebCryptoKeyFormatJwk: {
      // The JWK could represent either a public key or private key. The usages
      // must make sense for one of the two. The usages will be checked again by
      // ImportKeyJwk() once the key type has been determined.
      if (CheckKeyCreationUsages(all_public_key_usages, usages, true)
              .IsError() &&
          CheckKeyCreationUsages(all_private_key_usages, usages, false)
              .IsError()) {
        return Status::ErrorCreateKeyBadUsages();
      }
      return Status::Success();
    }
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

void TruncateToBitLength(size_t length_bits, std::vector<uint8_t>* bytes) {
  size_t length_bytes = NumBitsToBytes(length_bits);

  if (bytes->size() != length_bytes) {
    CHECK_LT(length_bytes, bytes->size());
    bytes->resize(length_bytes);
  }

  size_t remainder_bits = length_bits % 8;

  // Zero any "unused bits" in the final byte
  if (remainder_bits)
    (*bytes)[bytes->size() - 1] &= ~((0xFF) >> remainder_bits);
}


Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages) {
  Status status = CheckKeyCreationUsages(all_public_usages | all_private_usages,
                                         combined_usages, true);
  if (status.IsError())
    return status;

  *public_usages = combined_usages & all_public_usages;
  *private_usages = combined_usages & all_private_usages;

  if (*private_usages == 0)
    return Status::ErrorCreateKeyEmptyUsages();

  return Status::Success();
}

}  // namespace webcrypto
