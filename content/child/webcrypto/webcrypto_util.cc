// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/webcrypto_util.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
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
  // TODO(eroman): Fix handling of empty biginteger. http://crbug.com/373552
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

struct JwkToWebCryptoUsage {
  const char* const jwk_key_op;
  const blink::WebCryptoKeyUsage webcrypto_usage;
};

// Keep this ordered according to the definition
// order of WebCrypto's "recognized key usage
// values".
//
// This is not required for spec compliance,
// however it makes the ordering of key_ops match
// that of WebCrypto's Key.usages.
const JwkToWebCryptoUsage kJwkWebCryptoUsageMap[] = {
    {"encrypt", blink::WebCryptoKeyUsageEncrypt},
    {"decrypt", blink::WebCryptoKeyUsageDecrypt},
    {"sign", blink::WebCryptoKeyUsageSign},
    {"verify", blink::WebCryptoKeyUsageVerify},
    {"deriveKey", blink::WebCryptoKeyUsageDeriveKey},
    {"deriveBits", blink::WebCryptoKeyUsageDeriveBits},
    {"wrapKey", blink::WebCryptoKeyUsageWrapKey},
    {"unwrapKey", blink::WebCryptoKeyUsageUnwrapKey}};

// Modifies the input usage_mask by according to the key_op value.
bool JwkKeyOpToWebCryptoUsage(const std::string& key_op,
                              blink::WebCryptoKeyUsageMask* usage_mask) {
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (kJwkWebCryptoUsageMap[i].jwk_key_op == key_op) {
      *usage_mask |= kJwkWebCryptoUsageMap[i].webcrypto_usage;
      return true;
    }
  }
  return false;
}

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
Status GetWebCryptoUsagesFromJwkKeyOps(
    const base::ListValue* jwk_key_ops_value,
    blink::WebCryptoKeyUsageMask* usage_mask) {
  *usage_mask = 0;
  for (size_t i = 0; i < jwk_key_ops_value->GetSize(); ++i) {
    std::string key_op;
    if (!jwk_key_ops_value->GetString(i, &key_op)) {
      return Status::ErrorJwkPropertyWrongType(
          base::StringPrintf("key_ops[%d]", static_cast<int>(i)), "string");
    }
    // Unrecognized key_ops are silently skipped.
    ignore_result(JwkKeyOpToWebCryptoUsage(key_op, usage_mask));
  }
  return Status::Success();
}

// Composes a JWK key_ops List from a Web Crypto usage mask.
// Note: Caller must assume ownership of returned instance.
base::ListValue* CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usage_mask) {
  base::ListValue* jwk_key_ops = new base::ListValue();
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (usage_mask & kJwkWebCryptoUsageMap[i].webcrypto_usage)
      jwk_key_ops->AppendString(kJwkWebCryptoUsageMap[i].jwk_key_op);
  }
  return jwk_key_ops;
}

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacImportParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  DCHECK(id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         id == blink::WebCryptoAlgorithmIdRsaOaep);
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id, new blink::WebCryptoRsaHashedImportParams(CreateAlgorithm(hash_id)));
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

bool IsAlgorithmRsa(blink::WebCryptoAlgorithmId alg_id) {
  return alg_id == blink::WebCryptoAlgorithmIdRsaOaep ||
         alg_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
}

bool IsAlgorithmAsymmetric(blink::WebCryptoAlgorithmId alg_id) {
  // TODO(padolph): include all other asymmetric algorithms once they are
  // defined, e.g. EC and DH.
  return IsAlgorithmRsa(alg_id);
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

  return Status::ErrorGenerateKeyLength();
}

Status GetHmacKeyGenLengthInBits(const blink::WebCryptoHmacKeyGenParams* params,
                                 unsigned int* keylen_bits) {
  if (!params->hasLengthBits()) {
    switch (params->hash().id()) {
      case blink::WebCryptoAlgorithmIdSha1:
      case blink::WebCryptoAlgorithmIdSha256:
        *keylen_bits = 512;
        return Status::Success();
      case blink::WebCryptoAlgorithmIdSha384:
      case blink::WebCryptoAlgorithmIdSha512:
        *keylen_bits = 1024;
        return Status::Success();
      default:
        return Status::ErrorUnsupported();
    }
  }

  if (params->optionalLengthBits() % 8)
    return Status::ErrorGenerateKeyLength();

  *keylen_bits = params->optionalLengthBits();

  // TODO(eroman): NSS fails when generating a zero-length secret key.
  if (*keylen_bits == 0)
    return Status::ErrorGenerateKeyLength();

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
                              blink::WebCryptoKeyUsageMask actual_usages) {
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
                        params->publicExponent().size(),
                        public_exponent)) {
    return Status::ErrorGenerateKeyPublicExponent();
  }

  // OpenSSL hangs when given bad public exponents, whereas NSS simply fails. To
  // avoid feeding OpenSSL data that will hang use a whitelist.
  if (*public_exponent != 3 && *public_exponent != 65537)
    return Status::ErrorGenerateKeyPublicExponent();

  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
