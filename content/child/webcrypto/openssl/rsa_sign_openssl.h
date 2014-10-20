// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_SIGN_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_SIGN_OPENSSL_H_

#include <stdint.h>

#include <vector>

namespace blink {
class WebCryptoKey;
}

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

// Helper functions for doing RSA-SSA signing and verification
// (both PKCS1-v1_5 and PSS flavor).
//
// The salt length parameter is only relevant when the key is for RSA-PSS. In
// other cases it should be set to zero.

Status RsaSign(const blink::WebCryptoKey& key,
               unsigned int pss_salt_length_bytes,
               const CryptoData& data,
               std::vector<uint8_t>* buffer);

Status RsaVerify(const blink::WebCryptoKey& key,
                 unsigned int pss_salt_length_bytes,
                 const CryptoData& signature,
                 const CryptoData& data,
                 bool* signature_match);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_RSA_SIGN_OPENSSL_H_
