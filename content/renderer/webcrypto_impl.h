// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
#define CONTENT_RENDERER_WEBCRYPTO_IMPL_H_

#include "base/compiler_specific.h"

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

class CONTENT_EXPORT WebCryptoImpl
    : NON_EXPORTED_BASE(public WebKit::WebCrypto) {
 public:
  virtual void digest(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      size_t data_size,
      WebKit::WebCryptoResult result);

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebCryptoImplTest, DigestSampleSets);

  bool digestInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      size_t data_size,
      WebKit::WebArrayBuffer* buffer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
