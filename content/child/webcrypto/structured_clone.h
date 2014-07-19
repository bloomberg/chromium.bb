// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_STRUCTURED_CLONE_H_
#define CONTENT_CHILD_WEBCRYPTO_STRUCTURED_CLONE_H_

#include <stdint.h>

#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class CryptoData;

// Called on the target Blink thread.
bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          blink::WebVector<uint8_t>* key_data);

// Called on the target Blink thread.
bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usage_mask,
                            const CryptoData& key_data,
                            blink::WebCryptoKey* key);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_STRUCTURED_CLONE_H_
