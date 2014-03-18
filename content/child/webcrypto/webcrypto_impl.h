// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_
#define CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

// Wrapper around the blink WebCrypto asynchronous interface, which forwards to
// the synchronous platfrom (NSS or OpenSSL) implementation.
//
// TODO(eroman): Post the synchronous work to a background thread.
class WebCryptoImpl : public blink::WebCrypto {
 public:
  WebCryptoImpl();
  virtual ~WebCryptoImpl();

  virtual void encrypt(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* data,
                       unsigned int data_size,
                       blink::WebCryptoResult result);
  virtual void decrypt(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* data,
                       unsigned int data_size,
                       blink::WebCryptoResult result);
  virtual void digest(const blink::WebCryptoAlgorithm& algorithm,
                      const unsigned char* data,
                      unsigned int data_size,
                      blink::WebCryptoResult result);
  virtual void generateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           blink::WebCryptoResult result);
  virtual void importKey(blink::WebCryptoKeyFormat format,
                         const unsigned char* key_data,
                         unsigned int key_data_size,
                         const blink::WebCryptoAlgorithm& algorithm_or_null,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoResult result);
  virtual void exportKey(blink::WebCryptoKeyFormat format,
                         const blink::WebCryptoKey& key,
                         blink::WebCryptoResult result);
  virtual void sign(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& key,
                    const unsigned char* data,
                    unsigned int data_size,
                    blink::WebCryptoResult result);
  virtual void verifySignature(const blink::WebCryptoAlgorithm& algorithm,
                               const blink::WebCryptoKey& key,
                               const unsigned char* signature,
                               unsigned int signature_size,
                               const unsigned char* data,
                               unsigned int data_size,
                               blink::WebCryptoResult result);
  // This method synchronously computes a digest for the given data, returning
  // |true| if successful and |false| otherwise.
  virtual bool digestSynchronous(const blink::WebCryptoAlgorithmId algorithm_id,
                                 const unsigned char* data,
                                 unsigned int data_size,
                                 blink::WebArrayBuffer& result);

  virtual bool deserializeKeyForClone(
      const blink::WebCryptoKeyAlgorithm& algorithm,
      blink::WebCryptoKeyType type,
      bool extractable,
      blink::WebCryptoKeyUsageMask usages,
      const unsigned char* key_data,
      unsigned key_data_size,
      blink::WebCryptoKey& key);

  virtual bool serializeKeyForClone(const blink::WebCryptoKey& key,
                                    blink::WebVector<unsigned char>& key_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_
