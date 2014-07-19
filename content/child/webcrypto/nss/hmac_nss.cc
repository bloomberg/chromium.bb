// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <pk11pub.h>
#include <secerr.h>
#include <sechash.h>

#include "base/logging.h"
#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/sym_key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

const blink::WebCryptoKeyUsageMask kAllKeyUsages =
    blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

bool WebCryptoHashToHMACMechanism(const blink::WebCryptoAlgorithm& algorithm,
                                  CK_MECHANISM_TYPE* mechanism) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      *mechanism = CKM_SHA_1_HMAC;
      return true;
    case blink::WebCryptoAlgorithmIdSha256:
      *mechanism = CKM_SHA256_HMAC;
      return true;
    case blink::WebCryptoAlgorithmIdSha384:
      *mechanism = CKM_SHA384_HMAC;
      return true;
    case blink::WebCryptoAlgorithmIdSha512:
      *mechanism = CKM_SHA512_HMAC;
      return true;
    default:
      return false;
  }
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

    const blink::WebCryptoAlgorithm& hash = params->hash();
    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
    if (!WebCryptoHashToHMACMechanism(hash, &mechanism))
      return Status::ErrorUnsupported();

    unsigned int keylen_bits = 0;
    Status status = GetHmacKeyGenLengthInBits(params, &keylen_bits);
    if (status.IsError())
      return status;

    return GenerateSecretKeyNss(
        blink::WebCryptoKeyAlgorithm::createHmac(hash.id(), keylen_bits),
        extractable,
        usage_mask,
        keylen_bits / 8,
        mechanism,
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

    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
    if (!WebCryptoHashToHMACMechanism(hash, &mechanism))
      return Status::ErrorUnsupported();

    // TODO(eroman): check for overflow.
    unsigned int keylen_bits = key_data.byte_length() * 8;
    return ImportKeyRawNss(
        key_data,
        blink::WebCryptoKeyAlgorithm::createHmac(hash.id(), keylen_bits),
        extractable,
        usage_mask,
        mechanism,
        CKF_SIGN | CKF_VERIFY,
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
    *buffer = SymKeyNss::Cast(key)->raw_key_data();
    return Status::Success();
  }

  virtual Status ExportKeyJwk(const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const OVERRIDE {
    SymKeyNss* sym_key = SymKeyNss::Cast(key);
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
    PK11SymKey* sym_key = SymKeyNss::Cast(key)->key();

    CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
    if (!WebCryptoHashToHMACMechanism(hash, &mechanism))
      return Status::ErrorUnexpected();

    SECItem param_item = {siBuffer, NULL, 0};
    SECItem data_item = MakeSECItemForBuffer(data);
    // First call is to figure out the length.
    SECItem signature_item = {siBuffer, NULL, 0};

    if (PK11_SignWithSymKey(
            sym_key, mechanism, &param_item, &signature_item, &data_item) !=
        SECSuccess) {
      return Status::OperationError();
    }

    DCHECK_NE(0u, signature_item.len);

    buffer->resize(signature_item.len);
    signature_item.data = Uint8VectorStart(buffer);

    if (PK11_SignWithSymKey(
            sym_key, mechanism, &param_item, &signature_item, &data_item) !=
        SECSuccess) {
      return Status::OperationError();
    }

    CHECK_EQ(buffer->size(), signature_item.len);
    return Status::Success();
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
                       crypto::SecureMemEqual(Uint8VectorStart(result),
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
