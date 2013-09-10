// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include <sechash.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"

namespace content {

bool WebCryptoImpl::DigestInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  HASH_HashType hash_type = HASH_AlgNULL;

  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      hash_type = HASH_AlgSHA1;
      break;
    case WebKit::WebCryptoAlgorithmIdSha224:
      hash_type = HASH_AlgSHA224;
      break;
    case WebKit::WebCryptoAlgorithmIdSha256:
      hash_type = HASH_AlgSHA256;
      break;
    case WebKit::WebCryptoAlgorithmIdSha384:
      hash_type = HASH_AlgSHA384;
      break;
    case WebKit::WebCryptoAlgorithmIdSha512:
      hash_type = HASH_AlgSHA512;
      break;
    default:
      // Not a digest algorithm.
      return false;
  }

  crypto::EnsureNSSInit();

  HASHContext* context = HASH_Create(hash_type);
  if (!context) {
    return false;
  }

  HASH_Begin(context);

  HASH_Update(context, data, data_size);

  size_t hash_result_length = HASH_ResultLenContext(context);
  DCHECK_LE(hash_result_length, static_cast<size_t>(HASH_LENGTH_MAX));

  *buffer = WebKit::WebArrayBuffer::create(hash_result_length, 1);

  unsigned char* digest = reinterpret_cast<unsigned char*>(buffer->data());

  uint32 result_length = 0;
  HASH_End(context, digest, &result_length, hash_result_length);

  HASH_Destroy(context);

  return result_length == hash_result_length;
}

}  // namespace content
