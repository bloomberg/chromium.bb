// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/sym_key_openssl.h"

#include <vector>
#include <openssl/rand.h>

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

Status GenerateSecretKeyOpenSsl(const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usage_mask,
                                unsigned keylen_bytes,
                                blink::WebCryptoKey* key) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<unsigned char> random_bytes(keylen_bytes, 0);

  if (keylen_bytes > 0) {
    if (!(RAND_bytes(&random_bytes[0], keylen_bytes)))
      return Status::OperationError();
  }

  *key =
      blink::WebCryptoKey::create(new SymKeyOpenSsl(CryptoData(random_bytes)),
                                  blink::WebCryptoKeyTypeSecret,
                                  extractable,
                                  algorithm,
                                  usage_mask);
  return Status::Success();
}

Status ImportKeyRawOpenSsl(const CryptoData& key_data,
                           const blink::WebCryptoKeyAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::create(new SymKeyOpenSsl(key_data),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     algorithm,
                                     usage_mask);
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
