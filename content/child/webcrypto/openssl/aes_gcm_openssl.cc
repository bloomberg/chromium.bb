// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <openssl/evp.h>

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/aes_key_openssl.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

namespace {

const EVP_AEAD* GetAesGcmAlgorithmFromKeySize(unsigned int key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_gcm();
    // TODO(eroman): Hook up 256-bit support when it is available.
    default:
      return NULL;
  }
}

Status AesGcmEncryptDecrypt(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  const std::vector<uint8_t>& raw_key =
      SymKeyOpenSsl::Cast(key)->raw_key_data();
  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  unsigned int tag_length_bits;
  Status status = GetAesGcmTagLengthInBits(params, &tag_length_bits);
  if (status.IsError())
    return status;
  unsigned int tag_length_bytes = tag_length_bits / 8;

  CryptoData iv(params->iv());
  CryptoData additional_data(params->optionalAdditionalData());

  EVP_AEAD_CTX ctx;

  const EVP_AEAD* const aead_alg =
      GetAesGcmAlgorithmFromKeySize(raw_key.size());
  if (!aead_alg)
    return Status::ErrorUnexpected();

  if (!EVP_AEAD_CTX_init(&ctx,
                         aead_alg,
                         Uint8VectorStart(raw_key),
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
                           Uint8VectorStart(buffer),
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
    buffer->resize(data.byte_length() + tag_length_bytes);

    ok = EVP_AEAD_CTX_seal(&ctx,
                           Uint8VectorStart(buffer),
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

class AesGcmImplementation : public AesAlgorithm {
 public:
  AesGcmImplementation() : AesAlgorithm("GCM") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesGcmEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesGcmImplementation() {
  return new AesGcmImplementation;
}

}  // namespace webcrypto

}  // namespace content
