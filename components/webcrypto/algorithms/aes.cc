// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/aes.h"

#include "base/logging.h"
#include "components/webcrypto/algorithms/util_openssl.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/key.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Creates an AES algorithm name for the given key size (in bytes). For
// instance "A128CBC" is the result of suffix="CBC", keylen_bytes=16.
std::string MakeJwkAesAlgorithmName(const std::string& suffix,
                                    size_t keylen_bytes) {
  if (keylen_bytes == 16)
    return std::string("A128") + suffix;
  if (keylen_bytes == 24)
    return std::string("A192") + suffix;
  if (keylen_bytes == 32)
    return std::string("A256") + suffix;
  return std::string();
}

}  // namespace

AesAlgorithm::AesAlgorithm(blink::WebCryptoKeyUsageMask all_key_usages,
                           const std::string& jwk_suffix)
    : all_key_usages_(all_key_usages), jwk_suffix_(jwk_suffix) {
}

AesAlgorithm::AesAlgorithm(const std::string& jwk_suffix)
    : all_key_usages_(blink::WebCryptoKeyUsageEncrypt |
                      blink::WebCryptoKeyUsageDecrypt |
                      blink::WebCryptoKeyUsageWrapKey |
                      blink::WebCryptoKeyUsageUnwrapKey),
      jwk_suffix_(jwk_suffix) {
}

Status AesAlgorithm::GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 GenerateKeyResult* result) const {
  Status status = CheckKeyCreationUsages(all_key_usages_, usages, false);
  if (status.IsError())
    return status;

  unsigned int keylen_bits;
  status = GetAesKeyGenLengthInBits(algorithm.aesKeyGenParams(), &keylen_bits);
  if (status.IsError())
    return status;

  return GenerateWebCryptoSecretKey(
      blink::WebCryptoKeyAlgorithm::createAes(algorithm.id(), keylen_bits),
      extractable, usages, keylen_bits, result);
}

Status AesAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
    case blink::WebCryptoKeyFormatJwk:
      return CheckKeyCreationUsages(all_key_usages_, usages, false);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status AesAlgorithm::ImportKeyRaw(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  blink::WebCryptoKey* key) const {
  const unsigned int keylen_bytes = key_data.byte_length();
  Status status = VerifyAesKeyLengthForImport(keylen_bytes);
  if (status.IsError())
    return status;

  // No possibility of overflow.
  unsigned int keylen_bits = keylen_bytes * 8;

  return CreateWebCryptoSecretKey(
      key_data,
      blink::WebCryptoKeyAlgorithm::createAes(algorithm.id(), keylen_bits),
      extractable, usages, key);
}

Status AesAlgorithm::ImportKeyJwk(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  blink::WebCryptoKey* key) const {
  std::vector<uint8_t> raw_data;
  JwkReader jwk;
  Status status = ReadSecretKeyNoExpectedAlg(key_data, extractable, usages,
                                             &raw_data, &jwk);
  if (status.IsError())
    return status;

  bool has_jwk_alg;
  std::string jwk_alg;
  status = jwk.GetAlg(&jwk_alg, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg) {
    std::string expected_algorithm_name =
        MakeJwkAesAlgorithmName(jwk_suffix_, raw_data.size());

    if (jwk_alg != expected_algorithm_name) {
      // Give a different error message if the key length was wrong.
      if (jwk_alg == MakeJwkAesAlgorithmName(jwk_suffix_, 16) ||
          jwk_alg == MakeJwkAesAlgorithmName(jwk_suffix_, 24) ||
          jwk_alg == MakeJwkAesAlgorithmName(jwk_suffix_, 32)) {
        return Status::ErrorJwkIncorrectKeyLength();
      }
      return Status::ErrorJwkAlgorithmInconsistent();
    }
  }

  return ImportKeyRaw(CryptoData(raw_data), algorithm, extractable, usages,
                      key);
}

Status AesAlgorithm::ExportKeyRaw(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  *buffer = GetSymmetricKeyData(key);
  return Status::Success();
}

Status AesAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  const std::vector<uint8_t>& raw_data = GetSymmetricKeyData(key);

  WriteSecretKeyJwk(CryptoData(raw_data),
                    MakeJwkAesAlgorithmName(jwk_suffix_, raw_data.size()),
                    key.extractable(), key.usages(), buffer);

  return Status::Success();
}

Status AesAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) const {
  return ImportKeyRaw(key_data, CreateAlgorithm(algorithm.id()), extractable,
                      usages, key);
}

Status AesAlgorithm::GetKeyLength(
    const blink::WebCryptoAlgorithm& key_length_algorithm,
    bool* has_length_bits,
    unsigned int* length_bits) const {
  return GetAesKeyLength(key_length_algorithm, has_length_bits, length_bits);
}

}  // namespace webcrypto
