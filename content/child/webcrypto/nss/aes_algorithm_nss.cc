// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/aes_algorithm_nss.h"

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/sym_key_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

AesAlgorithm::AesAlgorithm(CK_MECHANISM_TYPE import_mechanism,
                           blink::WebCryptoKeyUsageMask all_key_usages,
                           const std::string& jwk_suffix)
    : import_mechanism_(import_mechanism),
      all_key_usages_(all_key_usages),
      jwk_suffix_(jwk_suffix) {
}

AesAlgorithm::AesAlgorithm(CK_MECHANISM_TYPE import_mechanism,
                           const std::string& jwk_suffix)
    : import_mechanism_(import_mechanism),
      all_key_usages_(blink::WebCryptoKeyUsageEncrypt |
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

  return GenerateSecretKeyNss(
      blink::WebCryptoKeyAlgorithm::createAes(algorithm.id(), keylen_bits),
      extractable, usages, keylen_bits, CKM_AES_KEY_GEN, result);
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

  return ImportKeyRawNss(key_data, blink::WebCryptoKeyAlgorithm::createAes(
                                       algorithm.id(), keylen_bits),
                         extractable, usages, import_mechanism_, key);
}

Status AesAlgorithm::ImportKeyJwk(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  blink::WebCryptoKey* key) const {
  std::vector<uint8_t> raw_data;
  Status status = ReadAesSecretKeyJwk(key_data, jwk_suffix_, extractable,
                                      usages, &raw_data);
  if (status.IsError())
    return status;

  return ImportKeyRaw(CryptoData(raw_data), algorithm, extractable, usages,
                      key);
}

Status AesAlgorithm::ExportKeyRaw(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  *buffer = SymKeyNss::Cast(key)->raw_key_data();
  return Status::Success();
}

Status AesAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  SymKeyNss* sym_key = SymKeyNss::Cast(key);
  const std::vector<uint8_t>& raw_data = sym_key->raw_key_data();

  WriteSecretKeyJwk(CryptoData(raw_data),
                    MakeJwkAesAlgorithmName(jwk_suffix_, raw_data.size()),
                    key.extractable(), key.usages(), buffer);

  return Status::Success();
}

Status AesAlgorithm::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<uint8_t>* key_data) const {
  key_data->assign(SymKeyNss::Cast(key)->serialized_key_data());
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

}  // namespace content
