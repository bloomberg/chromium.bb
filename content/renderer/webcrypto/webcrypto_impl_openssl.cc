// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <vector>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace {

class SymKeyHandle : public WebKit::WebCryptoKeyHandle {
 public:
  SymKeyHandle(const unsigned char* key_data, unsigned key_data_size)
      : key_(key_data, key_data + key_data_size) {}

  const std::vector<unsigned char>& key() const { return key_; }

 private:
  const std::vector<unsigned char> key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyHandle);
};

}  // anonymous namespace

void WebCryptoImpl::Init() { crypto::EnsureOpenSSLInit(); }

bool WebCryptoImpl::EncryptInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::DecryptInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::DigestInternal(const WebKit::WebCryptoAlgorithm& algorithm,
                                   const unsigned char* data,
                                   unsigned data_size,
                                   WebKit::WebArrayBuffer* buffer) {

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  const EVP_MD* digest_algorithm;
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      digest_algorithm = EVP_sha1();
      break;
    case WebKit::WebCryptoAlgorithmIdSha224:
      digest_algorithm = EVP_sha224();
      break;
    case WebKit::WebCryptoAlgorithmIdSha256:
      digest_algorithm = EVP_sha256();
      break;
    case WebKit::WebCryptoAlgorithmIdSha384:
      digest_algorithm = EVP_sha384();
      break;
    case WebKit::WebCryptoAlgorithmIdSha512:
      digest_algorithm = EVP_sha512();
      break;
    default:
      // Not a digest algorithm.
      return false;
  }

  crypto::ScopedOpenSSL<EVP_MD_CTX, EVP_MD_CTX_destroy> digest_context(
      EVP_MD_CTX_create());
  if (!digest_context.get()) {
    return false;
  }

  if (!EVP_DigestInit_ex(digest_context.get(), digest_algorithm, NULL) ||
      !EVP_DigestUpdate(digest_context.get(), data, data_size)) {
    return false;
  }

  const int hash_expected_size = EVP_MD_CTX_size(digest_context.get());
  if (hash_expected_size <= 0) {
    return false;
  }
  DCHECK_LE(hash_expected_size, EVP_MAX_MD_SIZE);

  *buffer = WebKit::WebArrayBuffer::create(hash_expected_size, 1);
  unsigned char* const hash_buffer =
      reinterpret_cast<unsigned char* const>(buffer->data());

  unsigned hash_size = 0;
  if (!EVP_DigestFinal_ex(digest_context.get(), hash_buffer, &hash_size) ||
      static_cast<int>(hash_size) != hash_expected_size) {
    buffer->reset();
    return false;
  }

  return true;
}

bool WebCryptoImpl::GenerateKeyInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    scoped_ptr<WebKit::WebCryptoKeyHandle>* key,
    WebKit::WebCryptoKeyType* type) {
  // TODO(ellyjones): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::ImportKeyInternal(
    WebKit::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const WebKit::WebCryptoAlgorithm& algorithm,
    WebKit::WebCryptoKeyUsageMask /*usage_mask*/,
    scoped_ptr<WebKit::WebCryptoKeyHandle>* handle,
    WebKit::WebCryptoKeyType* type) {

  // TODO(padolph): Support all relevant alg types and then remove this gate.
  if (algorithm.id() != WebKit::WebCryptoAlgorithmIdHmac &&
      algorithm.id() != WebKit::WebCryptoAlgorithmIdAesCbc) {
    return false;
  }

  // TODO(padolph): Need to split handling for symmetric (raw or jwk format) and
  // asymmetric (jwk, spki, or pkcs8 format) keys.
  // Currently only supporting symmetric.

  // TODO(padolph): jwk handling. Define precedence between jwk contents and
  // this method's parameters, e.g. 'alg' in jwk vs algorithm.id(). Who wins if
  // they differ? (jwk, probably)

  // Symmetric keys are always type secret
  *type = WebKit::WebCryptoKeyTypeSecret;

  const unsigned char* raw_key_data;
  unsigned raw_key_data_size;
  switch (format) {
    case WebKit::WebCryptoKeyFormatRaw:
      raw_key_data = key_data;
      raw_key_data_size = key_data_size;
      break;
    case WebKit::WebCryptoKeyFormatJwk:
      // TODO(padolph): Handle jwk format; need simple JSON parser.
      // break;
      return false;
    default:
      return false;
  }

  handle->reset(new SymKeyHandle(raw_key_data, raw_key_data_size));

  return true;
}

bool WebCryptoImpl::SignInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {

  WebKit::WebArrayBuffer result;

  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac: {

      DCHECK_EQ(key.algorithm().id(), WebKit::WebCryptoAlgorithmIdHmac);
      DCHECK_NE(0, key.usages() & WebKit::WebCryptoKeyUsageSign);

      const WebKit::WebCryptoHmacParams* const params = algorithm.hmacParams();
      if (!params)
        return false;

      const EVP_MD* evp_sha = 0;
      unsigned int hmac_expected_length = 0;
      // Note that HMAC length is determined by the hash used.
      switch (params->hash().id()) {
        case WebKit::WebCryptoAlgorithmIdSha1:
          evp_sha = EVP_sha1();
          hmac_expected_length = SHA_DIGEST_LENGTH;
          break;
        case WebKit::WebCryptoAlgorithmIdSha224:
          evp_sha = EVP_sha224();
          hmac_expected_length = SHA224_DIGEST_LENGTH;
          break;
        case WebKit::WebCryptoAlgorithmIdSha256:
          evp_sha = EVP_sha256();
          hmac_expected_length = SHA256_DIGEST_LENGTH;
          break;
        case WebKit::WebCryptoAlgorithmIdSha384:
          evp_sha = EVP_sha384();
          hmac_expected_length = SHA384_DIGEST_LENGTH;
          break;
        case WebKit::WebCryptoAlgorithmIdSha512:
          evp_sha = EVP_sha512();
          hmac_expected_length = SHA512_DIGEST_LENGTH;
          break;
        default:
          // Not a digest algorithm.
          return false;
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

      result = WebKit::WebArrayBuffer::create(hmac_expected_length, 1);
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
        return false;

      break;
    }
    default:
      return false;
  }

  *buffer = result;
  return true;
}

bool WebCryptoImpl::VerifySignatureInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    bool* signature_match) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac: {
      WebKit::WebArrayBuffer result;
      if (!SignInternal(algorithm, key, data, data_size, &result)) {
        return false;
      }

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
      return false;
  }
  return true;
}

}  // namespace content
