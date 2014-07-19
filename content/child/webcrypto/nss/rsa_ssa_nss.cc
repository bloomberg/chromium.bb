// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/nss/key_nss.h"
#include "content/child/webcrypto/nss/rsa_key_nss.h"
#include "content/child/webcrypto/nss/util_nss.h"
#include "content/child/webcrypto/status.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

class RsaSsaImplementation : public RsaHashedAlgorithm {
 public:
  RsaSsaImplementation()
      : RsaHashedAlgorithm(CKF_SIGN | CKF_VERIFY,
                           blink::WebCryptoKeyUsageVerify,
                           blink::WebCryptoKeyUsageSign) {}

  virtual const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const OVERRIDE {
    switch (hash) {
      case blink::WebCryptoAlgorithmIdSha1:
        return "RS1";
      case blink::WebCryptoAlgorithmIdSha256:
        return "RS256";
      case blink::WebCryptoAlgorithmIdSha384:
        return "RS384";
      case blink::WebCryptoAlgorithmIdSha512:
        return "RS512";
      default:
        return NULL;
    }
  }

  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    SECKEYPrivateKey* private_key = PrivateKeyNss::Cast(key)->key();

    const blink::WebCryptoAlgorithm& hash =
        key.algorithm().rsaHashedParams()->hash();

    // Pick the NSS signing algorithm by combining RSA-SSA (RSA PKCS1) and the
    // inner hash of the input Web Crypto algorithm.
    SECOidTag sign_alg_tag;
    switch (hash.id()) {
      case blink::WebCryptoAlgorithmIdSha1:
        sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
        break;
      case blink::WebCryptoAlgorithmIdSha256:
        sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
        break;
      case blink::WebCryptoAlgorithmIdSha384:
        sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
        break;
      case blink::WebCryptoAlgorithmIdSha512:
        sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
        break;
      default:
        return Status::ErrorUnsupported();
    }

    crypto::ScopedSECItem signature_item(SECITEM_AllocItem(NULL, NULL, 0));
    if (SEC_SignData(signature_item.get(),
                     data.bytes(),
                     data.byte_length(),
                     private_key,
                     sign_alg_tag) != SECSuccess) {
      return Status::OperationError();
    }

    buffer->assign(signature_item->data,
                   signature_item->data + signature_item->len);
    return Status::Success();
  }

  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        const CryptoData& signature,
                        const CryptoData& data,
                        bool* signature_match) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    SECKEYPublicKey* public_key = PublicKeyNss::Cast(key)->key();

    const blink::WebCryptoAlgorithm& hash =
        key.algorithm().rsaHashedParams()->hash();

    const SECItem signature_item = MakeSECItemForBuffer(signature);

    SECOidTag hash_alg_tag;
    switch (hash.id()) {
      case blink::WebCryptoAlgorithmIdSha1:
        hash_alg_tag = SEC_OID_SHA1;
        break;
      case blink::WebCryptoAlgorithmIdSha256:
        hash_alg_tag = SEC_OID_SHA256;
        break;
      case blink::WebCryptoAlgorithmIdSha384:
        hash_alg_tag = SEC_OID_SHA384;
        break;
      case blink::WebCryptoAlgorithmIdSha512:
        hash_alg_tag = SEC_OID_SHA512;
        break;
      default:
        return Status::ErrorUnsupported();
    }

    *signature_match =
        SECSuccess == VFY_VerifyDataDirect(data.bytes(),
                                           data.byte_length(),
                                           public_key,
                                           &signature_item,
                                           SEC_OID_PKCS1_RSA_ENCRYPTION,
                                           hash_alg_tag,
                                           NULL,
                                           NULL);
    return Status::Success();
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaSsaImplementation() {
  return new RsaSsaImplementation;
}

}  // namespace webcrypto

}  // namespace content
