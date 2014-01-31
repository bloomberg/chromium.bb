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
namespace webcrypto { class Status; }

class CONTENT_EXPORT WebCryptoImpl
    : NON_EXPORTED_BASE(public blink::WebCrypto) {
 public:
  WebCryptoImpl();

  virtual void encrypt(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebCryptoResult result);
  virtual void decrypt(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebCryptoResult result);
  virtual void digest(
      const blink::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebCryptoResult result);
  virtual void generateKey(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoResult result);
  virtual void importKey(
      blink::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned int key_data_size,
      const blink::WebCryptoAlgorithm& algorithm_or_null,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoResult result);
  virtual void exportKey(
      blink::WebCryptoKeyFormat format,
      const blink::WebCryptoKey& key,
      blink::WebCryptoResult result);
  virtual void sign(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebCryptoResult result);
  virtual void verifySignature(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned int signature_size,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebCryptoResult result);


 protected:
  friend class WebCryptoImplTest;

  void Init();

  webcrypto::Status EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer);
  webcrypto::Status DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer);
  webcrypto::Status DigestInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer);
  webcrypto::Status GenerateSecretKeyInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key);
  webcrypto::Status GenerateKeyPairInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key);
  webcrypto::Status ImportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const unsigned char* key_data,
      unsigned int key_data_size,
      const blink::WebCryptoAlgorithm& algorithm_or_null,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key);
  webcrypto::Status ExportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const blink::WebCryptoKey& key,
      blink::WebArrayBuffer* buffer);
  webcrypto::Status SignInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer);
  webcrypto::Status VerifySignatureInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned int signature_size,
      const unsigned char* data,
      unsigned int data_size,
      bool* signature_match);

  webcrypto::Status ImportKeyJwk(
      const unsigned char* key_data,
      unsigned int key_data_size,
      const blink::WebCryptoAlgorithm& algorithm_or_null,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key);
  webcrypto::Status ImportRsaPublicKeyInternal(
      const unsigned char* modulus_data,
      unsigned int modulus_size,
      const unsigned char* exponent_data,
      unsigned int exponent_size,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_IMPL_H_
