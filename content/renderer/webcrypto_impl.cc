// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include "third_party/WebKit/public/platform/WebArrayBuffer.h"

namespace content {

void WebCryptoImpl::digest(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    size_t data_size,
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!digestInternal(algorithm, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

}  // namespace content
