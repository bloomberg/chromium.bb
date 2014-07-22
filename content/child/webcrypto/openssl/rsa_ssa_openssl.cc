// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/rsa_key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Extracts the OpenSSL key and digest from a WebCrypto key. The returned
// pointers will remain valid as long as |key| is alive.
Status GetPKeyAndDigest(const blink::WebCryptoKey& key,
                        EVP_PKEY** pkey,
                        const EVP_MD** digest) {
  *pkey = AsymKeyOpenSsl::Cast(key)->key();

  *digest = GetDigest(key.algorithm().rsaHashedParams()->hash().id());
  if (!*digest)
    return Status::ErrorUnsupported();

  return Status::Success();
}

class RsaSsaImplementation : public RsaHashedAlgorithm {
 public:
  RsaSsaImplementation()
      : RsaHashedAlgorithm(blink::WebCryptoKeyUsageVerify,
                           blink::WebCryptoKeyUsageSign) {}

  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());

    EVP_PKEY* private_key = NULL;
    const EVP_MD* digest = NULL;
    Status status = GetPKeyAndDigest(key, &private_key, &digest);
    if (status.IsError())
      return status;

    // NOTE: A call to EVP_DigestSignFinal() with a NULL second parameter
    // returns a maximum allocation size, while the call without a NULL returns
    // the real one, which may be smaller.
    size_t sig_len = 0;
    if (!ctx.get() ||
        !EVP_DigestSignInit(ctx.get(), NULL, digest, NULL, private_key) ||
        !EVP_DigestSignUpdate(ctx.get(), data.bytes(), data.byte_length()) ||
        !EVP_DigestSignFinal(ctx.get(), NULL, &sig_len)) {
      return Status::OperationError();
    }

    buffer->resize(sig_len);
    if (!EVP_DigestSignFinal(ctx.get(), &buffer->front(), &sig_len))
      return Status::OperationError();

    buffer->resize(sig_len);
    return Status::Success();
  }

  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        const CryptoData& signature,
                        const CryptoData& data,
                        bool* signature_match) const OVERRIDE {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());

    EVP_PKEY* public_key = NULL;
    const EVP_MD* digest = NULL;
    Status status = GetPKeyAndDigest(key, &public_key, &digest);
    if (status.IsError())
      return status;

    if (1 != EVP_DigestVerifyInit(ctx.get(), NULL, digest, NULL, public_key))
      return Status::OperationError();

    if (1 !=
        EVP_DigestVerifyUpdate(ctx.get(), data.bytes(), data.byte_length())) {
      return Status::OperationError();
    }

    // This function takes a non-const pointer to the signature, however does
    // not mutate it, so casting is safe.
    // Also note that the return value can be:
    //   1 --> Success
    //   0 --> Verification failed
    //  <0 --> Operation error
    int rv = EVP_DigestVerifyFinal(ctx.get(),
                                   const_cast<uint8_t*>(signature.bytes()),
                                   signature.byte_length());
    *signature_match = rv == 1;
    return rv >= 0 ? Status::Success() : Status::OperationError();
  }
};

}  // namespace

AlgorithmImplementation* CreatePlatformRsaSsaImplementation() {
  return new RsaSsaImplementation;
}

}  // namespace webcrypto

}  // namespace content
