// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include "third_party/WebKit/public/platform/WebArrayBuffer.h"

namespace content {

WebCryptoImpl::WebCryptoImpl() {
  Init();
}

void WebCryptoImpl::digest(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!DigestInternal(algorithm, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

}  // namespace content
