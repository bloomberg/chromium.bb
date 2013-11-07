// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace {

bool IsAlgorithmAsymmetric(const blink::WebCryptoAlgorithm& algorithm) {
  // TODO(padolph): include all other asymmetric algorithms once they are
  // defined, e.g. EC and DH.
  return (algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaOaep);
}

}  // namespace

WebCryptoImpl::WebCryptoImpl() {
  Init();
}

// static
// TODO(eroman): This works by re-allocating a new buffer. It would be better if
//               the WebArrayBuffer could just be truncated instead.
void WebCryptoImpl::ShrinkBuffer(
    blink::WebArrayBuffer* buffer,
    unsigned new_size) {
  DCHECK_LE(new_size, buffer->byteLength());

  if (new_size == buffer->byteLength())
    return;

  blink::WebArrayBuffer new_buffer =
      blink::WebArrayBuffer::create(new_size, 1);
  DCHECK(!new_buffer.isNull());
  memcpy(new_buffer.data(), buffer->data(), new_size);
  *buffer = new_buffer;
}

void WebCryptoImpl::encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!EncryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!DecryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::digest(
    const blink::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!DigestInternal(algorithm, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::generateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  if (IsAlgorithmAsymmetric(algorithm)) {
    blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
    blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
    if (!GenerateKeyPairInternal(
             algorithm, extractable, usage_mask, &public_key, &private_key)) {
      result.completeWithError();
    } else {
      DCHECK(public_key.handle());
      DCHECK(private_key.handle());
      DCHECK_EQ(algorithm.id(), public_key.algorithm().id());
      DCHECK_EQ(algorithm.id(), private_key.algorithm().id());
      // TODO(padolph): The public key should probably always be extractable,
      // regardless of the input 'extractable' parameter, but that is not called
      // out in the Web Crypto API spec.
      // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23695
      DCHECK_EQ(extractable, public_key.extractable());
      DCHECK_EQ(extractable, private_key.extractable());
      DCHECK_EQ(usage_mask, public_key.usages());
      DCHECK_EQ(usage_mask, private_key.usages());
      result.completeWithKeyPair(public_key, private_key);
    }
  } else {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    if (!GenerateKeyInternal(algorithm, extractable, usage_mask, &key)) {
      result.completeWithError();
    } else {
      DCHECK(key.handle());
      DCHECK_EQ(algorithm.id(), key.algorithm().id());
      DCHECK_EQ(extractable, key.extractable());
      DCHECK_EQ(usage_mask, key.usages());
      result.completeWithKey(key);
    }
  }
}

void WebCryptoImpl::importKey(
    blink::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoResult result) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  if (!ImportKeyInternal(format,
                         key_data,
                         key_data_size,
                         algorithm_or_null,
                         extractable,
                         usage_mask,
                         &key)) {
    result.completeWithError();
    return;
  }
  DCHECK(key.handle());
  DCHECK(!key.algorithm().isNull());
  DCHECK_EQ(extractable, key.extractable());
  result.completeWithKey(key);
}

void WebCryptoImpl::sign(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!SignInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::verifySignature(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
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
