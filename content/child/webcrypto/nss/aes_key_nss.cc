// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/aes_key_nss.h"

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
                           CK_FLAGS import_flags,
                           blink::WebCryptoKeyUsageMask all_key_usages,
                           const std::string& jwk_suffix)
    : import_mechanism_(import_mechanism),
      import_flags_(import_flags),
      all_key_usages_(all_key_usages),
      jwk_suffix_(jwk_suffix) {
}

AesAlgorithm::AesAlgorithm(CK_MECHANISM_TYPE import_mechanism,
                           const std::string& jwk_suffix)
    : import_mechanism_(import_mechanism),
      import_flags_(CKF_ENCRYPT | CKF_DECRYPT),
      all_key_usages_(blink::WebCryptoKeyUsageEncrypt |
                      blink::WebCryptoKeyUsageDecrypt |
                      blink::WebCryptoKeyUsageWrapKey |
                      blink::WebCryptoKeyUsageUnwrapKey),
      jwk_suffix_(jwk_suffix) {
}

Status AesAlgorithm::VerifyKeyUsagesBeforeGenerateKey(
    blink::WebCryptoKeyUsageMask usage_mask) const {
  return CheckKeyCreationUsages(all_key_usages_, usage_mask);
}

Status AesAlgorithm::GenerateSecretKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  unsigned int keylen_bits;
  Status status =
      GetAesKeyGenLengthInBits(algorithm.aesKeyGenParams(), &keylen_bits);
  if (status.IsError())
    return status;

  return GenerateSecretKeyNss(
      blink::WebCryptoKeyAlgorithm::createAes(algorithm.id(), keylen_bits),
      extractable,
      usage_mask,
      keylen_bits / 8,
      CKM_AES_KEY_GEN,
      key);
}

Status AesAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usage_mask) const {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
    case blink::WebCryptoKeyFormatJwk:
      return CheckKeyCreationUsages(all_key_usages_, usage_mask);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}
Status AesAlgorithm::ImportKeyRaw(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usage_mask,
                                  blink::WebCryptoKey* key) const {
  const unsigned int keylen_bytes = key_data.byte_length();
  Status status = VerifyAesKeyLengthForImport(keylen_bytes);
  if (status.IsError())
    return status;

  // No possibility of overflow.
  unsigned int keylen_bits = keylen_bytes * 8;

  return ImportKeyRawNss(
      key_data,
      blink::WebCryptoKeyAlgorithm::createAes(algorithm.id(), keylen_bits),
      extractable,
      usage_mask,
      import_mechanism_,
      import_flags_,
      key);
}

Status AesAlgorithm::ImportKeyJwk(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usage_mask,
                                  blink::WebCryptoKey* key) const {
  std::vector<uint8_t> raw_data;
  Status status = ReadAesSecretKeyJwk(
      key_data, jwk_suffix_, extractable, usage_mask, &raw_data);
  if (status.IsError())
    return status;

  return ImportKeyRaw(
      CryptoData(raw_data), algorithm, extractable, usage_mask, key);
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
                    key.extractable(),
                    key.usages(),
                    buffer);

  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
