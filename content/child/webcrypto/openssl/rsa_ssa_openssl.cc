// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/rsa_key_openssl.h"
#include "content/child/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

class RsaSsaImplementation : public RsaHashedAlgorithm {
 public:
  RsaSsaImplementation()
      : RsaHashedAlgorithm(blink::WebCryptoKeyUsageVerify,
                           blink::WebCryptoKeyUsageSign) {}

  // TODO(eroman): Implement Sign() and Verify().
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaSsaImplementation() {
  return new RsaSsaImplementation;
}

}  // namespace webcrypto

}  // namespace content
