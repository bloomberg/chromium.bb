// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <vector>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "base/logging.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

using webcrypto::Status;

namespace {

class SymKeyHandle : public blink::WebCryptoKeyHandle {
 public:
  SymKeyHandle(const unsigned char* key_data, unsigned int key_data_size)
      : key_(key_data, key_data + key_data_size) {}

  const std::vector<unsigned char>& key() const { return key_; }

 private:
  const std::vector<unsigned char> key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyHandle);
};

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

// OpenSSL constants for EVP_CipherInit_ex(), do not change
enum CipherOperation {
  kDoDecrypt = 0,
  kDoEncrypt = 1
};

Status AesCbcEncryptDecrypt(CipherOperation cipher_operation,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const unsigned char* data,
                            unsigned int data_size,
                            blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdAesCbc, algorithm.id());
  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK_EQ(blink::WebCryptoKeyTypeSecret, key.type());

  if (data_size >= INT_MAX - AES_BLOCK_SIZE) {
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

  SymKeyHandle* const sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  const EVP_CIPHER* const cipher =
      GetAESCipherByKeyLength(sym_key->key().size());
  DCHECK(cipher);

  const blink::WebCryptoAesCbcParams* const params = algorithm.aesCbcParams();
  if (params->iv().size() != AES_BLOCK_SIZE)
    return Status::ErrorIncorrectSizeAesCbcIv();

  if (!EVP_CipherInit_ex(context.get(),
                         cipher,
                         NULL,
                         &sym_key->key()[0],
                         params->iv().data(),
                         cipher_operation)) {
    return Status::Error();
  }

  // According to the openssl docs, the amount of data written may be as large
  // as (data_size + cipher_block_size - 1), constrained to a multiple of
  // cipher_block_size.
  unsigned int output_max_len = data_size + AES_BLOCK_SIZE - 1;
  const unsigned remainder = output_max_len % AES_BLOCK_SIZE;
  if (remainder != 0)
    output_max_len += AES_BLOCK_SIZE - remainder;
  DCHECK_GT(output_max_len, data_size);

  *buffer = blink::WebArrayBuffer::create(output_max_len, 1);

  unsigned char* const buffer_data =
      reinterpret_cast<unsigned char*>(buffer->data());

  int output_len = 0;
  if (!EVP_CipherUpdate(
          context.get(), buffer_data, &output_len, data, data_size))
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

  webcrypto::ShrinkBuffer(buffer, final_output_len);

  return Status::Success();
}

Status ExportKeyInternalRaw(
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {

  DCHECK(key.handle());
  DCHECK(buffer);

  if (key.type() != blink::WebCryptoKeyTypeSecret)
    return Status::ErrorUnexpectedKeyType();

  // TODO(eroman): This should be in a more generic location.
  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();

  const SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  *buffer = webcrypto::CreateArrayBuffer(
      webcrypto::Uint8VectorStart(sym_key->key()), sym_key->key().size());

  return Status::Success();
}

}  // namespace

void WebCryptoImpl::Init() { crypto::EnsureOpenSSLInit(); }

Status WebCryptoImpl::EncryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  if (algorithm.id() == blink::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        kDoEncrypt, algorithm, key, data, data_size, buffer);
  }

  return Status::ErrorUnsupported();
}

Status WebCryptoImpl::DecryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  if (algorithm.id() == blink::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        kDoDecrypt, algorithm, key, data, data_size, buffer);
  }

  return Status::ErrorUnsupported();
}

Status WebCryptoImpl::DigestInternal(const blink::WebCryptoAlgorithm& algorithm,
                                     const unsigned char* data,
                                     unsigned int data_size,
                                     blink::WebArrayBuffer* buffer) {

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  const EVP_MD* digest_algorithm;
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      digest_algorithm = EVP_sha1();
      break;
    case blink::WebCryptoAlgorithmIdSha224:
      digest_algorithm = EVP_sha224();
      break;
    case blink::WebCryptoAlgorithmIdSha256:
      digest_algorithm = EVP_sha256();
      break;
    case blink::WebCryptoAlgorithmIdSha384:
      digest_algorithm = EVP_sha384();
      break;
    case blink::WebCryptoAlgorithmIdSha512:
      digest_algorithm = EVP_sha512();
      break;
    default:
      // Not a digest algorithm.
      return Status::ErrorUnsupported();
  }

  crypto::ScopedOpenSSL<EVP_MD_CTX, EVP_MD_CTX_destroy> digest_context(
      EVP_MD_CTX_create());
  if (!digest_context.get())
    return Status::Error();

  if (!EVP_DigestInit_ex(digest_context.get(), digest_algorithm, NULL) ||
      !EVP_DigestUpdate(digest_context.get(), data, data_size)) {
    return Status::Error();
  }

  const int hash_expected_size = EVP_MD_CTX_size(digest_context.get());
  if (hash_expected_size <= 0) {
    return Status::ErrorUnexpected();
  }
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

Status WebCryptoImpl::GenerateSecretKeyInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  unsigned int keylen_bytes = 0;
  blink::WebCryptoKeyType key_type;
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc: {
      const blink::WebCryptoAesKeyGenParams* params =
          algorithm.aesKeyGenParams();
      DCHECK(params);
      if (params->lengthBits() % 8)
        return Status::ErrorGenerateKeyLength();
      keylen_bytes = params->lengthBits() / 8;
      if (!GetAESCipherByKeyLength(keylen_bytes))
        return Status::Error();
      key_type = blink::WebCryptoKeyTypeSecret;
      break;
    }
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
      DCHECK(params);
      if (params->hasLengthBytes())
        keylen_bytes = params->optionalLengthBytes();
      else
        keylen_bytes = webcrypto::ShaBlockSizeBytes(params->hash().id());
      key_type = blink::WebCryptoKeyTypeSecret;
      break;
    }

    default: { return Status::ErrorUnsupported(); }
  }

  if (keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  std::vector<unsigned char> random_bytes(keylen_bytes, 0);
  if (!(RAND_bytes(&random_bytes[0], keylen_bytes)))
    return Status::Error();

  *key = blink::WebCryptoKey::create(
      new SymKeyHandle(&random_bytes[0], random_bytes.size()),
      key_type, extractable, algorithm, usage_mask);

  return Status::Success();
}

Status WebCryptoImpl::GenerateKeyPairInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return Status::ErrorUnsupported();
}

Status WebCryptoImpl::ImportKeyInternal(
    blink::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned int key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {
  // TODO(eroman): Currently expects algorithm to always be specified, as it is
  //               required for raw format.
  if (algorithm_or_null.isNull())
    return Status::ErrorMissingAlgorithmImportRawKey();
  const blink::WebCryptoAlgorithm& algorithm = algorithm_or_null;

  // TODO(padolph): Support all relevant alg types and then remove this gate.
  if (algorithm.id() != blink::WebCryptoAlgorithmIdHmac &&
      algorithm.id() != blink::WebCryptoAlgorithmIdAesCbc) {
    return Status::ErrorUnsupported();
  }

  // TODO(padolph): Need to split handling for symmetric (raw format) and
  // asymmetric (spki or pkcs8 format) keys.
  // Currently only supporting symmetric.

  // Symmetric keys are always type secret
  blink::WebCryptoKeyType type = blink::WebCryptoKeyTypeSecret;

  const unsigned char* raw_key_data;
  unsigned int raw_key_data_size;
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      raw_key_data = key_data;
      raw_key_data_size = key_data_size;
      // The NSS implementation fails when importing a raw AES key with a length
      // incompatible with AES. The line below is to match this behavior.
      if (algorithm.id() == blink::WebCryptoAlgorithmIdAesCbc &&
          !GetAESCipherByKeyLength(raw_key_data_size)) {
        return Status::Error();
      }
      break;
    case blink::WebCryptoKeyFormatJwk:
      return Status::ErrorUnexpected();
    default:
      return Status::ErrorUnsupported();
  }

  *key = blink::WebCryptoKey::create(
      new SymKeyHandle(raw_key_data, raw_key_data_size),
      type, extractable, algorithm, usage_mask);

  return Status::Success();
}

Status WebCryptoImpl::ExportKeyInternal(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return ExportKeyInternalRaw(key, buffer);
    case blink::WebCryptoKeyFormatSpki:
      // TODO(padolph): Implement spki export
      return Status::ErrorUnsupported();
    case blink::WebCryptoKeyFormatPkcs8:
      // TODO(padolph): Implement pkcs8 export
      return Status::ErrorUnsupported();
    default:
      return Status::ErrorUnsupported();
  }
  return Status::ErrorUnsupported();
}

Status WebCryptoImpl::SignInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {

  blink::WebArrayBuffer result;

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {

      DCHECK_EQ(key.algorithm().id(), blink::WebCryptoAlgorithmIdHmac);
      DCHECK_NE(0, key.usages() & blink::WebCryptoKeyUsageSign);

      const blink::WebCryptoHmacParams* const params = algorithm.hmacParams();
      if (!params)
        return Status::ErrorUnexpected();

      const EVP_MD* evp_sha = 0;
      unsigned int hmac_expected_length = 0;
      // Note that HMAC length is determined by the hash used.
      switch (params->hash().id()) {
        case blink::WebCryptoAlgorithmIdSha1:
          evp_sha = EVP_sha1();
          hmac_expected_length = SHA_DIGEST_LENGTH;
          break;
        case blink::WebCryptoAlgorithmIdSha224:
          evp_sha = EVP_sha224();
          hmac_expected_length = SHA224_DIGEST_LENGTH;
          break;
        case blink::WebCryptoAlgorithmIdSha256:
          evp_sha = EVP_sha256();
          hmac_expected_length = SHA256_DIGEST_LENGTH;
          break;
        case blink::WebCryptoAlgorithmIdSha384:
          evp_sha = EVP_sha384();
          hmac_expected_length = SHA384_DIGEST_LENGTH;
          break;
        case blink::WebCryptoAlgorithmIdSha512:
          evp_sha = EVP_sha512();
          hmac_expected_length = SHA512_DIGEST_LENGTH;
          break;
        default:
          // Not a digest algorithm.
          return Status::ErrorUnsupported();
      }

      SymKeyHandle* const sym_key =
          reinterpret_cast<SymKeyHandle*>(key.handle());
      const std::vector<unsigned char>& raw_key = sym_key->key();

      // OpenSSL wierdness here.
      // First, HMAC() needs a void* for the key data, so make one up front as a
      // cosmetic to avoid a cast. Second, OpenSSL does not like a NULL key,
      // which will result if the raw_key vector is empty; an entirely valid
      // case. Handle this specific case by pointing to an empty array.
      const unsigned char null_key[] = {};
      const void* const raw_key_voidp = raw_key.size() ? &raw_key[0] : null_key;

      result = blink::WebArrayBuffer::create(hmac_expected_length, 1);
      crypto::ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> hmac_result(
          reinterpret_cast<unsigned char*>(result.data()),
          hmac_expected_length);

      crypto::OpenSSLErrStackTracer(FROM_HERE);

      unsigned int hmac_actual_length;
      unsigned char* const success = HMAC(evp_sha,
                                          raw_key_voidp,
                                          raw_key.size(),
                                          data,
                                          data_size,
                                          hmac_result.safe_buffer(),
                                          &hmac_actual_length);
      if (!success || hmac_actual_length != hmac_expected_length)
        return Status::Error();

      break;
    }
    default:
      return Status::ErrorUnsupported();
  }

  *buffer = result;
  return Status::Success();
}

Status WebCryptoImpl::VerifySignatureInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned int signature_size,
    const unsigned char* data,
    unsigned int data_size,
    bool* signature_match) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {
      blink::WebArrayBuffer result;
      Status status = SignInternal(algorithm, key, data, data_size, &result);
      if (status.IsError())
        return status;

      // Handling of truncated signatures is underspecified in the WebCrypto
      // spec, so here we fail verification if a truncated signature is being
      // verified.
      // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23097
      *signature_match =
          result.byteLength() == signature_size &&
          crypto::SecureMemEqual(result.data(), signature, signature_size);

      break;
    }
    default:
      return Status::ErrorUnsupported();
  }
  return Status::Success();
}

Status WebCryptoImpl::ImportRsaPublicKeyInternal(
    const unsigned char* modulus_data,
    unsigned int modulus_size,
    const unsigned char* exponent_data,
    unsigned int exponent_size,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return Status::ErrorUnsupported();
}

}  // namespace content
