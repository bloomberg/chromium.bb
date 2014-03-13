// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/platform_crypto.h"

#include <vector>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace platform {

class SymKey : public Key {
 public:
  explicit SymKey(const CryptoData& key_data)
      : key_(key_data.bytes(), key_data.bytes() + key_data.byte_length()) {}

  virtual SymKey* AsSymKey() OVERRIDE { return this; }
  virtual PublicKey* AsPublicKey() OVERRIDE { return NULL; }
  virtual PrivateKey* AsPrivateKey() OVERRIDE { return NULL; }

  const std::vector<unsigned char>& key() const { return key_; }

 private:
  const std::vector<unsigned char> key_;

  DISALLOW_COPY_AND_ASSIGN(SymKey);
};

namespace {

const EVP_CIPHER* GetAESCipherByKeyLength(unsigned int key_length_bytes) {
  // OpenSSL supports AES CBC ciphers for only 3 key lengths: 128, 192, 256 bits
  switch (key_length_bytes) {
    case 16:
      return EVP_aes_128_cbc();
    case 24:
      return EVP_aes_192_cbc();
    case 32:
      return EVP_aes_256_cbc();
    default:
      return NULL;
  }
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

// OpenSSL constants for EVP_CipherInit_ex(), do not change
enum CipherOperation { kDoDecrypt = 0, kDoEncrypt = 1 };

Status AesCbcEncryptDecrypt(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& iv,
                            const CryptoData& data,
                            blink::WebArrayBuffer* buffer) {
  CipherOperation cipher_operation =
      (mode == ENCRYPT) ? kDoEncrypt : kDoDecrypt;

  if (data.byte_length() >= INT_MAX - AES_BLOCK_SIZE) {
    // TODO(padolph): Handle this by chunking the input fed into OpenSSL. Right
    // now it doesn't make much difference since the one-shot API would end up
    // blowing out the memory and crashing anyway.
    return Status::ErrorDataTooLarge();
  }

  // Note: PKCS padding is enabled by default
  crypto::ScopedOpenSSL<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context(
      EVP_CIPHER_CTX_new());

  if (!context.get())
    return Status::Error();

  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(key->key().size());
  DCHECK(cipher);

  if (!EVP_CipherInit_ex(context.get(),
                         cipher,
                         NULL,
                         &key->key()[0],
                         iv.bytes(),
                         cipher_operation)) {
    return Status::Error();
  }

  // According to the openssl docs, the amount of data written may be as large
  // as (data_size + cipher_block_size - 1), constrained to a multiple of
  // cipher_block_size.
  unsigned int output_max_len = data.byte_length() + AES_BLOCK_SIZE - 1;
  const unsigned remainder = output_max_len % AES_BLOCK_SIZE;
  if (remainder != 0)
    output_max_len += AES_BLOCK_SIZE - remainder;
  DCHECK_GT(output_max_len, data.byte_length());

  *buffer = blink::WebArrayBuffer::create(output_max_len, 1);

  unsigned char* const buffer_data =
      reinterpret_cast<unsigned char*>(buffer->data());

  int output_len = 0;
  if (!EVP_CipherUpdate(context.get(),
                        buffer_data,
                        &output_len,
                        data.bytes(),
                        data.byte_length()))
    return Status::Error();
  int final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex(
          context.get(), buffer_data + output_len, &final_output_chunk_len)) {
    return Status::Error();
  }

  const unsigned int final_output_len =
      static_cast<unsigned int>(output_len) +
      static_cast<unsigned int>(final_output_chunk_len);
  DCHECK_LE(final_output_len, output_max_len);

  ShrinkBuffer(buffer, final_output_len);

  return Status::Success();
}

}  // namespace

Status ExportKeyRaw(SymKey* key, blink::WebArrayBuffer* buffer) {
  *buffer = CreateArrayBuffer(Uint8VectorStart(key->key()), key->key().size());
  return Status::Success();
}

void Init() { crypto::EnsureOpenSSLInit(); }

Status EncryptDecryptAesCbc(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            blink::WebArrayBuffer* buffer) {
  // TODO(eroman): inline the function here.
  return AesCbcEncryptDecrypt(mode, key, iv, data, buffer);
}

Status DigestSha(blink::WebCryptoAlgorithmId algorithm,
                 const CryptoData& data,
                 blink::WebArrayBuffer* buffer) {
  crypto::OpenSSLErrStackTracer(FROM_HERE);

  const EVP_MD* digest_algorithm = GetDigest(algorithm);
  if (!digest_algorithm)
    return Status::ErrorUnexpected();

  crypto::ScopedOpenSSL<EVP_MD_CTX, EVP_MD_CTX_destroy> digest_context(
      EVP_MD_CTX_create());
  if (!digest_context.get())
    return Status::Error();

  if (!EVP_DigestInit_ex(digest_context.get(), digest_algorithm, NULL) ||
      !EVP_DigestUpdate(
          digest_context.get(), data.bytes(), data.byte_length())) {
    return Status::Error();
  }

  const int hash_expected_size = EVP_MD_CTX_size(digest_context.get());
  if (hash_expected_size <= 0)
    return Status::ErrorUnexpected();
  DCHECK_LE(hash_expected_size, EVP_MAX_MD_SIZE);

  *buffer = blink::WebArrayBuffer::create(hash_expected_size, 1);
  unsigned char* const hash_buffer =
      reinterpret_cast<unsigned char* const>(buffer->data());

  unsigned int hash_size = 0;
  if (!EVP_DigestFinal_ex(digest_context.get(), hash_buffer, &hash_size) ||
      static_cast<int>(hash_size) != hash_expected_size) {
    buffer->reset();
    return Status::Error();
  }

  return Status::Success();
}

Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         unsigned keylen_bytes,
                         blink::WebCryptoKey* key) {
  // TODO(eroman): Is this right?
  if (keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  std::vector<unsigned char> random_bytes(keylen_bytes, 0);
  if (!(RAND_bytes(&random_bytes[0], keylen_bytes)))
    return Status::Error();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateSecretKeyAlgorithm(algorithm, keylen_bytes, &key_algorithm))
    return Status::ErrorUnexpected();

  *key = blink::WebCryptoKey::create(new SymKey(CryptoData(random_bytes)),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status GenerateRsaKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          unsigned int modulus_length_bits,
                          const CryptoData& public_exponent,
                          const blink::WebCryptoAlgorithm& hash,
                          blink::WebCryptoKey* public_key,
                          blink::WebCryptoKey* private_key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return Status::ErrorUnsupported();
}

Status ImportKeyRaw(const blink::WebCryptoAlgorithm& algorithm,
                    const CryptoData& key_data,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateSecretKeyAlgorithm(
          algorithm, key_data.byte_length(), &key_algorithm))
    return Status::ErrorUnexpected();

  *key = blink::WebCryptoKey::create(new SymKey(key_data),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status SignHmac(SymKey* key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                blink::WebArrayBuffer* buffer) {
  blink::WebArrayBuffer result;

  const EVP_MD* digest_algorithm = GetDigest(hash.id());
  if (!digest_algorithm)
    return Status::ErrorUnsupported();
  unsigned int hmac_expected_length = EVP_MD_size(digest_algorithm);

  const std::vector<unsigned char>& raw_key = key->key();

  // OpenSSL wierdness here.
  // First, HMAC() needs a void* for the key data, so make one up front as a
  // cosmetic to avoid a cast. Second, OpenSSL does not like a NULL key,
  // which will result if the raw_key vector is empty; an entirely valid
  // case. Handle this specific case by pointing to an empty array.
  const unsigned char null_key[] = {};
  const void* const raw_key_voidp = raw_key.size() ? &raw_key[0] : null_key;

  result = blink::WebArrayBuffer::create(hmac_expected_length, 1);
  crypto::ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> hmac_result(
      reinterpret_cast<unsigned char*>(result.data()), hmac_expected_length);

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  unsigned int hmac_actual_length;
  unsigned char* const success = HMAC(digest_algorithm,
                                      raw_key_voidp,
                                      raw_key.size(),
                                      data.bytes(),
                                      data.byte_length(),
                                      hmac_result.safe_buffer(),
                                      &hmac_actual_length);
  if (!success || hmac_actual_length != hmac_expected_length)
    return Status::Error();

  *buffer = result;
  return Status::Success();
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          const CryptoData& modulus_data,
                          const CryptoData& exponent_data,
                          blink::WebCryptoKey* key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue
  return Status::ErrorUnsupported();
}

Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            const CryptoData& additional_data,
                            unsigned int tag_length_bits,
                            blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

// Guaranteed that key is valid.
Status EncryptRsaEsPkcs1v1_5(PublicKey* key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status DecryptRsaEsPkcs1v1_5(PrivateKey* key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status SignRsaSsaPkcs1v1_5(PrivateKey* key,
                           const blink::WebCryptoAlgorithm& hash,
                           const CryptoData& data,
                           blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

// Key is guaranteed to be an RSA SSA key.
Status VerifyRsaSsaPkcs1v1_5(PublicKey* key,
                             const blink::WebCryptoAlgorithm& hash,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ImportKeySpki(const blink::WebCryptoAlgorithm& algorithm_or_null,
                     const CryptoData& key_data,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usage_mask,
                     blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ImportKeyPkcs8(const blink::WebCryptoAlgorithm& algorithm_or_null,
                      const CryptoData& key_data,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usage_mask,
                      blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ExportKeySpki(PublicKey* key, blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status WrapSymKeyAesKw(SymKey* wrapping_key,
                       SymKey* key,
                       blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status UnwrapSymKeyAesKw(const CryptoData& wrapped_key_data,
                         SymKey* wrapping_key,
                         const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status WrapSymKeyRsaEs(PublicKey* wrapping_key,
                       SymKey* key,
                       blink::WebArrayBuffer* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status UnwrapSymKeyRsaEs(const CryptoData& wrapped_key_data,
                         PrivateKey* wrapping_key,
                         const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

}  // namespace platform

}  // namespace webcrypto

}  // namespace content
