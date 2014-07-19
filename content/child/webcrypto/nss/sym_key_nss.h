// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_NSS_SYM_KEY_NSS_H_
#define CONTENT_CHILD_WEBCRYPTO_NSS_SYM_KEY_NSS_H_

#include <pkcs11t.h>

#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

Status GenerateSecretKeyNss(const blink::WebCryptoKeyAlgorithm& algorithm,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usage_mask,
                            unsigned keylen_bytes,
                            CK_MECHANISM_TYPE mechanism,
                            blink::WebCryptoKey* key);

Status ImportKeyRawNss(const CryptoData& key_data,
                       const blink::WebCryptoKeyAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usage_mask,
                       CK_MECHANISM_TYPE mechanism,
                       CK_FLAGS flags,
                       blink::WebCryptoKey* key);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_NSS_SYM_KEY_NSS_H_
