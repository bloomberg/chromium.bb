// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/algorithm_registry.h"

#include "base/lazy_instance.h"
#include "content/child/webcrypto/algorithm_implementation.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

namespace {

// This class is used as a singleton. All methods must be threadsafe.
class AlgorithmRegistry {
 public:
  AlgorithmRegistry()
      : sha_(CreatePlatformShaImplementation()),
        aes_gcm_(CreatePlatformAesGcmImplementation()),
        aes_cbc_(CreatePlatformAesCbcImplementation()),
        aes_ctr_(CreatePlatformAesCtrImplementation()),
        aes_kw_(CreatePlatformAesKwImplementation()),
        hmac_(CreatePlatformHmacImplementation()),
        rsa_ssa_(CreatePlatformRsaSsaImplementation()),
        rsa_oaep_(CreatePlatformRsaOaepImplementation()),
        rsa_pss_(CreatePlatformRsaPssImplementation()),
        ecdsa_(CreatePlatformEcdsaImplementation()),
        ecdh_(CreatePlatformEcdhImplementation()),
        hkdf_(CreatePlatformHkdfImplementation()),
        pbkdf2_(CreatePlatformPbkdf2Implementation()) {
    PlatformInit();
  }

  const AlgorithmImplementation* GetAlgorithm(
      blink::WebCryptoAlgorithmId id) const {
    switch (id) {
      case blink::WebCryptoAlgorithmIdSha1:
      case blink::WebCryptoAlgorithmIdSha256:
      case blink::WebCryptoAlgorithmIdSha384:
      case blink::WebCryptoAlgorithmIdSha512:
        return sha_.get();
      case blink::WebCryptoAlgorithmIdAesGcm:
        return aes_gcm_.get();
      case blink::WebCryptoAlgorithmIdAesCbc:
        return aes_cbc_.get();
      case blink::WebCryptoAlgorithmIdAesCtr:
        return aes_ctr_.get();
      case blink::WebCryptoAlgorithmIdAesKw:
        return aes_kw_.get();
      case blink::WebCryptoAlgorithmIdHmac:
        return hmac_.get();
      case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return rsa_ssa_.get();
      case blink::WebCryptoAlgorithmIdRsaOaep:
        return rsa_oaep_.get();
      case blink::WebCryptoAlgorithmIdRsaPss:
        return rsa_pss_.get();
      case blink::WebCryptoAlgorithmIdEcdsa:
        return ecdsa_.get();
      case blink::WebCryptoAlgorithmIdEcdh:
        return ecdh_.get();
      case blink::WebCryptoAlgorithmIdHkdf:
        return hkdf_.get();
      case blink::WebCryptoAlgorithmIdPbkdf2:
        return pbkdf2_.get();
      default:
        return NULL;
    }
  }

 private:
  const scoped_ptr<AlgorithmImplementation> sha_;
  const scoped_ptr<AlgorithmImplementation> aes_gcm_;
  const scoped_ptr<AlgorithmImplementation> aes_cbc_;
  const scoped_ptr<AlgorithmImplementation> aes_ctr_;
  const scoped_ptr<AlgorithmImplementation> aes_kw_;
  const scoped_ptr<AlgorithmImplementation> hmac_;
  const scoped_ptr<AlgorithmImplementation> rsa_ssa_;
  const scoped_ptr<AlgorithmImplementation> rsa_oaep_;
  const scoped_ptr<AlgorithmImplementation> rsa_pss_;
  const scoped_ptr<AlgorithmImplementation> ecdsa_;
  const scoped_ptr<AlgorithmImplementation> ecdh_;
  const scoped_ptr<AlgorithmImplementation> hkdf_;
  const scoped_ptr<AlgorithmImplementation> pbkdf2_;
};

}  // namespace

base::LazyInstance<AlgorithmRegistry>::Leaky g_algorithm_registry =
    LAZY_INSTANCE_INITIALIZER;

Status GetAlgorithmImplementation(blink::WebCryptoAlgorithmId id,
                                  const AlgorithmImplementation** impl) {
  *impl = g_algorithm_registry.Get().GetAlgorithm(id);
  if (*impl)
    return Status::Success();
  return Status::ErrorUnsupported();
}

}  // namespace webcrypto

}  // namespace content
