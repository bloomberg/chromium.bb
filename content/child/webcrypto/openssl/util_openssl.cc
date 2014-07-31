// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/util_openssl.h"

#include <openssl/evp.h>

#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"
#include "crypto/openssl_util.h"

namespace content {

namespace webcrypto {

void PlatformInit() {
  crypto::EnsureOpenSSLInit();
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::WebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::WebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::WebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::WebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return NULL;
  }
}

Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          const std::vector<uint8_t>& raw_key,
                          const CryptoData& data,
                          unsigned int tag_length_bytes,
                          const CryptoData& iv,
                          const CryptoData& additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  EVP_AEAD_CTX ctx;

  if (!aead_alg)
    return Status::ErrorUnexpected();

  if (!EVP_AEAD_CTX_init(&ctx,
                         aead_alg,
                         vector_as_array(&raw_key),
                         raw_key.size(),
                         tag_length_bytes,
                         NULL)) {
    return Status::OperationError();
  }

  crypto::ScopedOpenSSL<EVP_AEAD_CTX, EVP_AEAD_CTX_cleanup>::Type ctx_cleanup(
      &ctx);

  size_t len;
  int ok;

  if (mode == DECRYPT) {
    if (data.byte_length() < tag_length_bytes)
      return Status::ErrorDataTooSmall();

    buffer->resize(data.byte_length() - tag_length_bytes);

    ok = EVP_AEAD_CTX_open(&ctx,
                           vector_as_array(buffer),
                           &len,
                           buffer->size(),
                           iv.bytes(),
                           iv.byte_length(),
                           data.bytes(),
                           data.byte_length(),
                           additional_data.bytes(),
                           additional_data.byte_length());
  } else {
    // No need to check for unsigned integer overflow here (seal fails if
    // the output buffer is too small).
    buffer->resize(data.byte_length() + EVP_AEAD_max_overhead(aead_alg));

    ok = EVP_AEAD_CTX_seal(&ctx,
                           vector_as_array(buffer),
                           &len,
                           buffer->size(),
                           iv.bytes(),
                           iv.byte_length(),
                           data.bytes(),
                           data.byte_length(),
                           additional_data.bytes(),
                           additional_data.byte_length());
  }

  if (!ok)
    return Status::OperationError();
  buffer->resize(len);
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
