// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
#define CONTENT_RENDERER_WEBCRYPTO_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

class CONTENT_EXPORT WebCryptoImpl
    : NON_EXPORTED_BASE(public WebKit::WebCrypto) {
 public:
  WebCryptoImpl();

  virtual void digest(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);
  virtual void importKey(
      WebKit::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned key_data_size,
      const WebKit::WebCryptoAlgorithm& algorithm,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoResult result);
  virtual void sign(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebCryptoImplTest, DigestSampleSets);
  FRIEND_TEST_ALL_PREFIXES(WebCryptoImplTest, HMACSampleSets);

  void Init();

  bool DigestInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);
  bool ImportKeyInternal(
      WebKit::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned key_data_size,
      const WebKit::WebCryptoAlgorithm& algorithm,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      scoped_ptr<WebKit::WebCryptoKeyHandle>* handle,
      WebKit::WebCryptoKeyType* type);
  bool SignInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_IMPL_H_
