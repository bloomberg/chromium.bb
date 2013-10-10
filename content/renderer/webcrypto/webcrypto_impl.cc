// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

WebCryptoImpl::WebCryptoImpl() {
  Init();
}

void WebCryptoImpl::encrypt(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!EncryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::decrypt(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebCryptoResult result) {
  WebKit::WebArrayBuffer buffer;
  if (!DecryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
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

void WebCryptoImpl::generateKey(
    const WebKit::WebCryptoAlgorithm& algorithm,
    bool exportable,
    WebKit::WebCryptoKeyUsageMask usage,
    WebKit::WebCryptoResult result) {
  scoped_ptr<WebKit::WebCryptoKeyHandle> handle;
  WebKit::WebCryptoKeyType type;
  if (!GenerateKeyInternal(algorithm, &handle, &type)) {
    result.completeWithError();
  } else {
    WebKit::WebCryptoKey key(
        WebKit::WebCryptoKey::create(handle.release(), type, exportable,
                                     algorithm, usage));
    result.completeWithKey(key);
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

void WebCryptoImpl::verifySignature(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebCryptoResult result) {
  bool signature_match = false;
  if (!VerifySignatureInternal(algorithm,
                               key,
                               signature,
                               signature_size,
                               data,
                               data_size,
                               &signature_match)) {
    result.completeWithError();
  } else {
    result.completeWithBoolean(signature_match);
  }
}

}  // namespace content
