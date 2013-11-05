// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_IMPL_H_
#define CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

class CONTENT_EXPORT WebCryptoImpl
    : NON_EXPORTED_BASE(public WebKit::WebCrypto) {
 public:
  WebCryptoImpl();

  virtual void encrypt(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);
  virtual void decrypt(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);
  virtual void digest(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);
  virtual void generateKey(
      const WebKit::WebCryptoAlgorithm& algorithm,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoResult result);
  virtual void importKey(
      WebKit::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned key_data_size,
      const WebKit::WebCryptoAlgorithm& algorithm_or_null,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoResult result);
  virtual void sign(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);
  virtual void verifySignature(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned signature_size,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebCryptoResult result);

  static void ShrinkBuffer(WebKit::WebArrayBuffer* buffer, unsigned new_size);

 protected:
  friend class WebCryptoImplTest;

  void Init();

  bool EncryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);
  bool DecryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);
  bool DigestInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);
  bool GenerateKeyInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoKey* key);
  bool GenerateKeyPairInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoKey* public_key,
      WebKit::WebCryptoKey* private_key);
  bool ImportKeyInternal(
      WebKit::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned key_data_size,
      const WebKit::WebCryptoAlgorithm& algorithm_or_null,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoKey* key);
  bool SignInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer);
  bool VerifySignatureInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned signature_size,
      const unsigned char* data,
      unsigned data_size,
      bool* signature_match);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_IMPL_H_
