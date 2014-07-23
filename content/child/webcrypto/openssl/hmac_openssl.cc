// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/hmac.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/sym_key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

const blink::WebCryptoKeyUsageMask kAllKeyUsages =
    blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

Status SignHmac(const std::vector<uint8_t>& raw_key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* digest_algorithm = GetDigest(hash.id());
  if (!digest_algorithm)
    return Status::ErrorUnsupported();
  unsigned int hmac_expected_length = EVP_MD_size(digest_algorithm);

  // OpenSSL wierdness here.
  // First, HMAC() needs a void* for the key data, so make one up front as a
  // cosmetic to avoid a cast. Second, OpenSSL does not like a NULL key,
  // which will result if the raw_key vector is empty; an entirely valid
  // case. Handle this specific case by pointing to an empty array.
  const unsigned char null_key[] = {};
  const void* const raw_key_voidp = raw_key.size() ? &raw_key[0] : null_key;

  buffer->resize(hmac_expected_length);
  crypto::ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> hmac_result(
      vector_as_array(buffer), hmac_expected_length);

  unsigned int hmac_actual_length;
  unsigned char* const success = HMAC(digest_algorithm,
                                      raw_key_voidp,
                                      raw_key.size(),
                                      data.bytes(),
                                      data.byte_length(),
                                      hmac_result.safe_buffer(),
                                      &hmac_actual_length);
  if (!success || hmac_actual_length != hmac_expected_length)
    return Status::OperationError();

  return Status::Success();
}

class HmacImplementation : public AlgorithmImplementation {
 public:
  HmacImplementation() {}

  virtual Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                                   bool extractable,
                                   blink::WebCryptoKeyUsageMask usage_mask,
                                   blink::WebCryptoKey* key) const OVERRIDE {
    const blink::WebCryptoHmacKeyGenParams* params =
        algorithm.hmacKeyGenParams();

    unsigned int keylen_bits = 0;
    Status status = GetHmacKeyGenLengthInBits(params, &keylen_bits);
    if (status.IsError())
      return status;

    return GenerateSecretKeyOpenSsl(blink::WebCryptoKeyAlgorithm::createHmac(
                                        params->hash().id(), keylen_bits),
                                    extractable,
                                    usage_mask,
                                    keylen_bits / 8,
                                    key);
  }

  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE {
    switch (format) {
      case blink::WebCryptoKeyFormatRaw:
      case blink::WebCryptoKeyFormatJwk:
        return CheckKeyCreationUsages(kAllKeyUsages, usage_mask);
      default:
        return Status::ErrorUnsupportedImportKeyFormat();
    }
  }

  virtual Status VerifyKeyUsagesBeforeGenerateKey(
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE {
    return CheckKeyCreationUsages(kAllKeyUsages, usage_mask);
  }

  virtual Status ImportKeyRaw(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const OVERRIDE {
    const blink::WebCryptoAlgorithm& hash =
        algorithm.hmacImportParams()->hash();

    // TODO(eroman): check for overflow.
    unsigned int keylen_bits = key_data.byte_length() * 8;

    return ImportKeyRawOpenSsl(
        key_data,
        blink::WebCryptoKeyAlgorithm::createHmac(hash.id(), keylen_bits),
        extractable,
        usage_mask,
        key);
  }

  virtual Status ImportKeyJwk(const CryptoData& key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usage_mask,
                              blink::WebCryptoKey* key) const OVERRIDE {
    const char* algorithm_name =
        GetJwkHmacAlgorithmName(algorithm.hmacImportParams()->hash().id());
    if (!algorithm_name)
      return Status::ErrorUnexpected();

    std::vector<uint8_t> raw_data;
    Status status = ReadSecretKeyJwk(
        key_data, algorithm_name, extractable, usage_mask, &raw_data);
    if (status.IsError())
      return status;

    return ImportKeyRaw(
        CryptoData(raw_data), algorithm, extractable, usage_mask, key);
  }

  virtual Status ExportKeyRaw(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE {
    *buffer = SymKeyOpenSsl::Cast(key)->raw_key_data();
    return Status::Success();
  }

  virtual Status ExportKeyJwk(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE {
    SymKeyOpenSsl* sym_key = SymKeyOpenSsl::Cast(key);
    const std::vector<uint8_t>& raw_data = sym_key->raw_key_data();

    const char* algorithm_name =
        GetJwkHmacAlgorithmName(key.algorithm().hmacParams()->hash().id());
    if (!algorithm_name)
      return Status::ErrorUnexpected();

    WriteSecretKeyJwk(CryptoData(raw_data),
                      algorithm_name,
                      key.extractable(),
                      key.usages(),
                      buffer);

    return Status::Success();
  }

  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) const OVERRIDE {
    const blink::WebCryptoAlgorithm& hash =
        key.algorithm().hmacParams()->hash();

    return SignHmac(
        SymKeyOpenSsl::Cast(key)->raw_key_data(), hash, data, buffer);
  }

  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        const CryptoData& signature,
                        const CryptoData& data,
                        bool* signature_match) const OVERRIDE {
    std::vector<uint8_t> result;
    Status status = Sign(algorithm, key, data, &result);

    if (status.IsError())
      return status;

    // Do not allow verification of truncated MACs.
    *signature_match = result.size() == signature.byte_length() &&
                       crypto::SecureMemEqual(vector_as_array(&result),
                                              signature.bytes(),
                                              signature.byte_length());

    return Status::Success();
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformHmacImplementation() {
  return new HmacImplementation;
}

}  // namespace webcrypto

}  // namespace content
