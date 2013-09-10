// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include "third_party/WebKit/public/platform/WebArrayBuffer.h"

namespace content {

void WebCryptoImpl::digest(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
#ifdef WEBCRYPTO_DIGEST_LENGTH_IS_UINT
    unsigned data_size,
#else
    size_t data_size,
#endif
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!DigestInternal(algorithm, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

}  // namespace content
