// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_

#include <openssl/ossl_typ.h>

#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"

namespace content {

namespace webcrypto {

class CryptoData;

enum EncryptOrDecrypt { ENCRYPT, DECRYPT };

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_
