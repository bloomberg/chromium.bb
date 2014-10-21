// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_SYM_KEY_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_SYM_KEY_OPENSSL_H_

#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class CryptoData;
class GenerateKeyResult;
class Status;

Status GenerateSecretKeyOpenSsl(const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                unsigned keylen_bytes,
                                GenerateKeyResult* result);

Status ImportKeyRawOpenSsl(const CryptoData& key_data,
                           const blink::WebCryptoKeyAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           blink::WebCryptoKey* key);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_SYM_KEY_OPENSSL_H_
