// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/rsa_hashed_algorithm_openssl.h"
#include "content/child/webcrypto/openssl/rsa_sign_openssl.h"
#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

namespace {

class RsaSsaImplementation : public RsaHashedAlgorithm {
 public:
  RsaSsaImplementation()
      : RsaHashedAlgorithm(blink::WebCryptoKeyUsageVerify,
                           blink::WebCryptoKeyUsageSign) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const override {
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

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) const override {
    return RsaSign(key, 0, data, buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& signature,
                const CryptoData& data,
                bool* signature_match) const override {
    return RsaVerify(key, 0, signature, data, signature_match);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaSsaImplementation() {
  return new RsaSsaImplementation;
}

}  // namespace webcrypto

}  // namespace content
