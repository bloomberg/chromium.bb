// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
#define CONTENT_RENDERER_WEBCRYPTO_IMPL_H_

#include "base/compiler_specific.h"

#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

class WebCryptoImpl : public WebKit::WebCrypto {
 public:
  virtual WebKit::WebCryptoOperation* digest(
      const WebKit::WebCryptoAlgorithm& algorithm) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
