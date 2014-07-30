// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/evp.h>

#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/rsa_key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

typedef int (*InitFunc)(EVP_PKEY_CTX* ctx);
typedef int (*EncryptDecryptFunc)(EVP_PKEY_CTX* ctx,
                                  unsigned char* out,
                                  size_t* outlen,
                                  const unsigned char* in,
                                  size_t inlen);

// Helper for doing either RSA-OAEP encryption or decryption.
//
// To encrypt call with:
//   init_func=EVP_PKEY_encrypt_init, encrypt_decrypt_func=EVP_PKEY_encrypt
//
// To decrypt call with:
//   init_func=EVP_PKEY_decrypt_init, encrypt_decrypt_func=EVP_PKEY_decrypt
Status CommonEncryptDecrypt(InitFunc init_func,
                            EncryptDecryptFunc encrypt_decrypt_func,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = AsymKeyOpenSsl::Cast(key)->key();
  const EVP_MD* digest =
      GetDigest(key.algorithm().rsaHashedParams()->hash().id());
  if (!digest)
    return Status::ErrorUnsupported();

  crypto::ScopedEVP_PKEY_CTX ctx(EVP_PKEY_CTX_new(pkey, NULL));

  if (1 != init_func(ctx.get()) ||
      1 != EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) ||
      1 != EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), digest) ||
      1 != EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), digest)) {
    return Status::OperationError();
  }

  const blink::WebVector<uint8_t>& label =
      algorithm.rsaOaepParams()->optionalLabel();

  if (label.size()) {
    // Make a copy of the label, since the ctx takes ownership of it when
    // calling set0_rsa_oaep_label().
    crypto::ScopedOpenSSLBytes label_copy;
    label_copy.reset(static_cast<uint8_t*>(OPENSSL_malloc(label.size())));
    memcpy(label_copy.get(), label.data(), label.size());

    if (1 != EVP_PKEY_CTX_set0_rsa_oaep_label(
                 ctx.get(), label_copy.release(), label.size())) {
      return Status::OperationError();
    }
  }

  // Determine the maximum length of the output.
  size_t outlen = 0;
  if (1 != encrypt_decrypt_func(
               ctx.get(), NULL, &outlen, data.bytes(), data.byte_length())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  // Do the actual encryption/decryption.
  if (1 != encrypt_decrypt_func(ctx.get(),
                                vector_as_array(buffer),
                                &outlen,
                                data.bytes(),
                                data.byte_length())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  return Status::Success();
}

class RsaOaepImplementation : public RsaHashedAlgorithm {
 public:
  RsaOaepImplementation()
      : RsaHashedAlgorithm(
            blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageWrapKey,
            blink::WebCryptoKeyUsageDecrypt |
                blink::WebCryptoKeyUsageUnwrapKey) {}

  virtual const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const OVERRIDE {
    switch (hash) {
      case blink::WebCryptoAlgorithmIdSha1:
        return "RSA-OAEP";
      case blink::WebCryptoAlgorithmIdSha256:
        return "RSA-OAEP-256";
      case blink::WebCryptoAlgorithmIdSha384:
        return "RSA-OAEP-384";
      case blink::WebCryptoAlgorithmIdSha512:
        return "RSA-OAEP-512";
      default:
        return NULL;
    }
  }

  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(
        EVP_PKEY_encrypt_init, EVP_PKEY_encrypt, algorithm, key, data, buffer);
  }

  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(
        EVP_PKEY_decrypt_init, EVP_PKEY_decrypt, algorithm, key, data, buffer);
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaOaepImplementation() {
  return new RsaOaepImplementation;
}

}  // namespace webcrypto

}  // namespace content
