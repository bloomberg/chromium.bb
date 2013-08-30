// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

namespace content {

bool WebCryptoImpl::digestInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    size_t data_size,
    WebKit::WebArrayBuffer* buffer) {
  // TODO(bryaneyler): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

}  // namespace content
