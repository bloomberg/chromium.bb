// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/aes.h>
#include <openssl/evp.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/aes_key_openssl.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

namespace {

const EVP_CIPHER* GetAESCipherByKeyLength(unsigned int key_length_bytes) {
  // BoringSSL does not support 192-bit AES keys.
  switch (key_length_bytes) {
    case 16:
      return EVP_aes_128_cbc();
    case 32:
      return EVP_aes_256_cbc();
    default:
      return NULL;
  }
}

// OpenSSL constants for EVP_CipherInit_ex(), do not change
enum CipherOperation { kDoDecrypt = 0, kDoEncrypt = 1 };

Status AesCbcEncryptDecrypt(CipherOperation cipher_operation,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  const std::vector<uint8_t>& raw_key =
      SymKeyOpenSsl::Cast(key)->raw_key_data();

  if (params->iv().size() != 16)
    return Status::ErrorIncorrectSizeAesCbcIv();

  // According to the openssl docs, the amount of data written may be as large
  // as (data_size + cipher_block_size - 1), constrained to a multiple of
  // cipher_block_size.
  base::CheckedNumeric<int> output_max_len = data.byte_length();
  output_max_len += AES_BLOCK_SIZE - 1;
  if (!output_max_len.IsValid())
    return Status::ErrorDataTooLarge();

  const unsigned remainder = output_max_len.ValueOrDie() % AES_BLOCK_SIZE;
  if (remainder != 0)
    output_max_len += AES_BLOCK_SIZE - remainder;
  if (!output_max_len.IsValid())
    return Status::ErrorDataTooLarge();

  // Note: PKCS padding is enabled by default
  crypto::ScopedOpenSSL<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>::Type context(
      EVP_CIPHER_CTX_new());

  if (!context.get())
    return Status::OperationError();

  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(raw_key.size());
  DCHECK(cipher);

  if (!EVP_CipherInit_ex(context.get(),
                         cipher,
                         NULL,
                         &raw_key[0],
                         params->iv().data(),
                         cipher_operation)) {
    return Status::OperationError();
  }

  buffer->resize(output_max_len.ValueOrDie());

  unsigned char* const buffer_data = vector_as_array(buffer);

  int output_len = 0;
  if (!EVP_CipherUpdate(context.get(),
                        buffer_data,
                        &output_len,
                        data.bytes(),
                        data.byte_length()))
    return Status::OperationError();
  int final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex(
          context.get(), buffer_data + output_len, &final_output_chunk_len)) {
    return Status::OperationError();
  }

  const unsigned int final_output_len =
      static_cast<unsigned int>(output_len) +
      static_cast<unsigned int>(final_output_chunk_len);

  buffer->resize(final_output_len);

  return Status::Success();
}

class AesCbcImplementation : public AesAlgorithm {
 public:
  AesCbcImplementation() : AesAlgorithm("CBC") {}

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesCbcEncryptDecrypt(kDoEncrypt, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    return AesCbcEncryptDecrypt(kDoDecrypt, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformAesCbcImplementation() {
  return new AesCbcImplementation;
}

}  // namespace webcrypto

}  // namespace content
