// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/util_openssl.h"

#include <openssl/evp.h>

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "crypto/openssl_util.h"

namespace content {

namespace webcrypto {

void PlatformInit() {
  crypto::EnsureOpenSSLInit();
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::WebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::WebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::WebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::WebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return NULL;
  }
}

AlgorithmImplementation* CreatePlatformRsaOaepImplementation() {
  // TODO(eroman):
  return NULL;
}

}  // namespace webcrypto

}  // namespace content
