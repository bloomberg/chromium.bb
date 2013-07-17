// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"

namespace content {

WebKit::WebCryptoOperation* WebCryptoImpl::digest(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
    case WebKit::WebCryptoAlgorithmIdSha224:
    case WebKit::WebCryptoAlgorithmIdSha256:
    case WebKit::WebCryptoAlgorithmIdSha384:
    case WebKit::WebCryptoAlgorithmIdSha512:
      // TODO(eroman): Implement.
      return NULL;
    default:
      // Not a digest algorithm.
      return NULL;
  }
}

}  // namespace content
