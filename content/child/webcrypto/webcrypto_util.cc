// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/webcrypto_util.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "content/child/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned int.
bool BigIntegerToUint(const uint8_t* data,
                      unsigned int data_size,
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

Status GetShaBlockSizeBits(const blink::WebCryptoAlgorithm& algorithm,
                           unsigned int* block_size_bits) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha256:
      *block_size_bits = 512;
      return Status::Success();
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      *block_size_bits = 1024;
      return Status::Success();
    default:
      return Status::ErrorUnsupported();
  }
}

}  // namespace

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int length_bits) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacImportParams(CreateAlgorithm(hash_id), true,
                                           length_bits));
}

blink::WebCryptoAlgorithm CreateHmacImportAlgorithmNoLength(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacImportParams(CreateAlgorithm(hash_id), false, 0));
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

// TODO(eroman): Move this helper to WebCryptoKey.
bool KeyUsageAllows(const blink::WebCryptoKey& key,
                    const blink::WebCryptoKeyUsage usage) {
  return ((key.usages() & usage) != 0);
}

// The WebCrypto spec defines the default value for the tag length, as well as
// the allowed values for tag length.
Status GetAesGcmTagLengthInBits(const blink::WebCryptoAesGcmParams* params,
                                unsigned int* tag_length_bits) {
  *tag_length_bits = 128;
  if (params->hasTagLengthBits())
    *tag_length_bits = params->optionalTagLengthBits();

  if (*tag_length_bits != 32 && *tag_length_bits != 64 &&
      *tag_length_bits != 96 && *tag_length_bits != 104 &&
      *tag_length_bits != 112 && *tag_length_bits != 120 &&
      *tag_length_bits != 128)
    return Status::ErrorInvalidAesGcmTagLength();

  return Status::Success();
}

Status GetAesKeyGenLengthInBits(const blink::WebCryptoAesKeyGenParams* params,
                                unsigned int* keylen_bits) {
  *keylen_bits = params->lengthBits();

  if (*keylen_bits == 128 || *keylen_bits == 256)
    return Status::Success();

  // BoringSSL does not support 192-bit AES.
  if (*keylen_bits == 192)
    return Status::ErrorAes192BitUnsupported();

  return Status::ErrorGenerateAesKeyLength();
}

Status GetHmacKeyGenLengthInBits(const blink::WebCryptoHmacKeyGenParams* params,
                                 unsigned int* keylen_bits) {
  if (!params->hasLengthBits())
    return GetShaBlockSizeBits(params->hash(), keylen_bits);

  *keylen_bits = params->optionalLengthBits();

  // Zero-length HMAC keys are disallowed by the spec.
  if (*keylen_bits == 0)
    return Status::ErrorGenerateHmacKeyLengthZero();

  return Status::Success();
}

Status GetHmacImportKeyLengthBits(
    const blink::WebCryptoHmacImportParams* params,
    unsigned int key_data_byte_length,
    unsigned int* keylen_bits) {
  if (key_data_byte_length == 0)
    return Status::ErrorHmacImportEmptyKey();

  // Make sure that the key data's length can be represented in bits without
  // overflow.
  base::CheckedNumeric<unsigned int> checked_keylen_bits(key_data_byte_length);
  checked_keylen_bits *= 8;

  if (!checked_keylen_bits.IsValid())
    return Status::ErrorDataTooLarge();

  unsigned int data_keylen_bits = checked_keylen_bits.ValueOrDie();

  // Determine how many bits of the input to use.
  *keylen_bits = data_keylen_bits;
  if (params->hasLengthBits()) {
    // The requested bit length must be:
    //   * No longer than the input data length
    //   * At most 7 bits shorter.
    if (NumBitsToBytes(params->optionalLengthBits()) != key_data_byte_length)
      return Status::ErrorHmacImportBadLength();
    *keylen_bits = params->optionalLengthBits();
  }

  return Status::Success();
}

Status VerifyAesKeyLengthForImport(unsigned int keylen_bytes) {
  if (keylen_bytes == 16 || keylen_bytes == 32)
    return Status::Success();

  // BoringSSL does not support 192-bit AES.
  if (keylen_bytes == 24)
    return Status::ErrorAes192BitUnsupported();

  return Status::ErrorImportAesKeyLength();
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

  // Limit key sizes to those supported by NSS:
  //   * Multiple of 8 bits
  //   * 256 bits to 16K bits
  if (*modulus_length_bits < 256 || *modulus_length_bits > 16384 ||
      (*modulus_length_bits % 8) != 0) {
    return Status::ErrorGenerateRsaUnsupportedModulus();
  }

  if (!BigIntegerToUint(params->publicExponent().data(),
                        params->publicExponent().size(), public_exponent)) {
    return Status::ErrorGenerateKeyPublicExponent();
  }

  // OpenSSL hangs when given bad public exponents, whereas NSS simply fails. To
  // avoid feeding OpenSSL data that will hang use a whitelist.
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
      if (CheckKeyCreationUsages(
              all_public_key_usages, usages, true).IsError() &&
          CheckKeyCreationUsages(
              all_private_key_usages, usages, false).IsError()) {
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

Status GetAesKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                       bool* has_length_bits,
                       unsigned int* length_bits) {
  const blink::WebCryptoAesDerivedKeyParams* params =
      key_length_algorithm.aesDerivedKeyParams();

  *has_length_bits = true;
  *length_bits = params->lengthBits();

  if (*length_bits == 128 || *length_bits == 256)
    return Status::Success();

  // BoringSSL does not support 192-bit AES.
  if (*length_bits == 192)
    return Status::ErrorAes192BitUnsupported();

  return Status::ErrorGetAesKeyLength();
}

Status GetHmacKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                        bool* has_length_bits,
                        unsigned int* length_bits) {
  const blink::WebCryptoHmacImportParams* params =
      key_length_algorithm.hmacImportParams();

  if (params->hasLengthBits()) {
    *has_length_bits = true;
    *length_bits = params->optionalLengthBits();
    if (*length_bits == 0)
      return Status::ErrorGetHmacKeyLengthZero();
    return Status::Success();
  }

  *has_length_bits = true;
  return GetShaBlockSizeBits(params->hash(), length_bits);
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

}  // namespace content
