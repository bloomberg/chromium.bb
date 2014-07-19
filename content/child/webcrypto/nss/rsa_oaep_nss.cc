// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secerr.h>
#include <sechash.h>

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/rsa_key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

Status NssSupportsRsaOaep() {
  if (NssRuntimeSupport::Get()->IsRsaOaepSupported())
    return Status::Success();
  return Status::ErrorUnsupported(
      "NSS version doesn't support RSA-OAEP. Try using version 3.16.2 or "
      "later");
}

CK_MECHANISM_TYPE WebCryptoHashToMGFMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return CKG_MGF1_SHA1;
    case blink::WebCryptoAlgorithmIdSha256:
      return CKG_MGF1_SHA256;
    case blink::WebCryptoAlgorithmIdSha384:
      return CKG_MGF1_SHA384;
    case blink::WebCryptoAlgorithmIdSha512:
      return CKG_MGF1_SHA512;
    default:
      return CKM_INVALID_MECHANISM;
  }
}

CK_MECHANISM_TYPE WebCryptoHashToDigestMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1;
    case blink::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256;
    case blink::WebCryptoAlgorithmIdSha384:
      return CKM_SHA384;
    case blink::WebCryptoAlgorithmIdSha512:
      return CKM_SHA512;
    default:
      // Not a supported algorithm.
      return CKM_INVALID_MECHANISM;
  }
}

bool InitializeRsaOaepParams(const blink::WebCryptoAlgorithm& hash,
                             const CryptoData& label,
                             CK_RSA_PKCS_OAEP_PARAMS* oaep_params) {
  oaep_params->source = CKZ_DATA_SPECIFIED;
  oaep_params->pSourceData = const_cast<unsigned char*>(label.bytes());
  oaep_params->ulSourceDataLen = label.byte_length();
  oaep_params->mgf = WebCryptoHashToMGFMechanism(hash);
  oaep_params->hashAlg = WebCryptoHashToDigestMechanism(hash);

  if (oaep_params->mgf == CKM_INVALID_MECHANISM ||
      oaep_params->hashAlg == CKM_INVALID_MECHANISM) {
    return false;
  }

  return true;
}

Status EncryptRsaOaep(SECKEYPublicKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) {
  CK_RSA_PKCS_OAEP_PARAMS oaep_params = {0};
  if (!InitializeRsaOaepParams(hash, label, &oaep_params))
    return Status::ErrorUnsupported();

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&oaep_params);
  param.len = sizeof(oaep_params);

  buffer->resize(SECKEY_PublicKeyStrength(key));
  unsigned char* buffer_data = Uint8VectorStart(buffer);
  unsigned int output_len;
  if (NssRuntimeSupport::Get()->pk11_pub_encrypt_func()(key,
                                                        CKM_RSA_PKCS_OAEP,
                                                        &param,
                                                        buffer_data,
                                                        &output_len,
                                                        buffer->size(),
                                                        data.bytes(),
                                                        data.byte_length(),
                                                        NULL) != SECSuccess) {
    return Status::OperationError();
  }

  CHECK_LE(output_len, buffer->size());
  buffer->resize(output_len);
  return Status::Success();
}

Status DecryptRsaOaep(SECKEYPrivateKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) {
  Status status = NssSupportsRsaOaep();
  if (status.IsError())
    return status;

  CK_RSA_PKCS_OAEP_PARAMS oaep_params = {0};
  if (!InitializeRsaOaepParams(hash, label, &oaep_params))
    return Status::ErrorUnsupported();

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&oaep_params);
  param.len = sizeof(oaep_params);

  const int modulus_length_bytes = PK11_GetPrivateModulusLen(key);
  if (modulus_length_bytes <= 0)
    return Status::ErrorUnexpected();

  buffer->resize(modulus_length_bytes);

  unsigned char* buffer_data = Uint8VectorStart(buffer);
  unsigned int output_len;
  if (NssRuntimeSupport::Get()->pk11_priv_decrypt_func()(key,
                                                         CKM_RSA_PKCS_OAEP,
                                                         &param,
                                                         buffer_data,
                                                         &output_len,
                                                         buffer->size(),
                                                         data.bytes(),
                                                         data.byte_length()) !=
      SECSuccess) {
    return Status::OperationError();
  }

  CHECK_LE(output_len, buffer->size());
  buffer->resize(output_len);
  return Status::Success();
}

class RsaOaepImplementation : public RsaHashedAlgorithm {
 public:
  RsaOaepImplementation()
      : RsaHashedAlgorithm(
            CKF_ENCRYPT | CKF_DECRYPT | CKF_WRAP | CKF_UNWRAP,
            blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageWrapKey,
            blink::WebCryptoKeyUsageDecrypt |
                blink::WebCryptoKeyUsageUnwrapKey) {}

  virtual Status VerifyKeyUsagesBeforeGenerateKeyPair(
      blink::WebCryptoKeyUsageMask combined_usage_mask,
      blink::WebCryptoKeyUsageMask* public_usage_mask,
      blink::WebCryptoKeyUsageMask* private_usage_mask) const OVERRIDE {
    Status status = NssSupportsRsaOaep();
    if (status.IsError())
      return status;
    return RsaHashedAlgorithm::VerifyKeyUsagesBeforeGenerateKeyPair(
        combined_usage_mask, public_usage_mask, private_usage_mask);
  }

  virtual Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usage_mask) const OVERRIDE {
    Status status = NssSupportsRsaOaep();
    if (status.IsError())
      return status;
    return RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(format,
                                                              usage_mask);
  }

  virtual const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const OVERRIDE {
    switch (hash) {
      case blink::WebCryptoAlgorithmIdSha1:
        return "RSA-OAEP";
      case blink::WebCryptoAlgorithmIdSha256:
        return "RSA-OAEP-256";
      case blink::WebCryptoAlgorithmIdSha384:
        return "RSA-OAEP-384";
      case blink::WebCryptoAlgorithmIdSha512:
        return "RSA-OAEP-512";
      default:
        return NULL;
    }
  }

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    return EncryptRsaOaep(
        PublicKeyNss::Cast(key)->key(),
        key.algorithm().rsaHashedParams()->hash(),
        CryptoData(algorithm.rsaOaepParams()->optionalLabel()),
        data,
        buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    return DecryptRsaOaep(
        PrivateKeyNss::Cast(key)->key(),
        key.algorithm().rsaHashedParams()->hash(),
        CryptoData(algorithm.rsaOaepParams()->optionalLabel()),
        data,
        buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaOaepImplementation() {
  return new RsaOaepImplementation;
}

}  // namespace webcrypto

}  // namespace content
