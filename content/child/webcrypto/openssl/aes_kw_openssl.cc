// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/child/webcrypto/openssl/aes_key_openssl.h"
#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

namespace {

class AesKwImplementation : public AesAlgorithm {
 public:
  AesKwImplementation() : AesAlgorithm("KW") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    // TODO(eroman):
    return Status::ErrorUnsupported();
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    // TODO(eroman):
    return Status::ErrorUnsupported();
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesKwImplementation() {
  return new AesKwImplementation;
}

}  // namespace webcrypto

}  // namespace content
