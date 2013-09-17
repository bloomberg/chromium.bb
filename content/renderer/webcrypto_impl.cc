// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

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

void WebCryptoImpl::importKey(
    WebKit::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const WebKit::WebCryptoAlgorithm& algorithm,
    bool extractable,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    WebKit::WebCryptoResult result) {
  WebKit::WebCryptoKeyType type;
  scoped_ptr<WebKit::WebCryptoKeyHandle> handle;

  if (!ImportKeyInternal(format,
                         key_data,
                         key_data_size,
                         algorithm,
                         usage_mask,
                         &handle,
                         &type)) {
    result.completeWithError();
    return;
  }

  WebKit::WebCryptoKey key(
      WebKit::WebCryptoKey::create(
          handle.release(), type, extractable, algorithm, usage_mask));

  result.completeWithKey(key);
}

void WebCryptoImpl::sign(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!SignInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

}  // namespace content
