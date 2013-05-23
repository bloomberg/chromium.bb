// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_SCOPED_EVP_CIPHER_CTX_H_
#define NET_QUIC_CRYPTO_SCOPED_EVP_CIPHER_CTX_H_

typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

namespace net {

// TODO(wtc): this is the same as the ScopedCipherCTX class defined in
// crypto/encryptor_openssl.cc.  Eliminate the duplicate code.
// crypto::ScopedOpenSSL is not suitable for EVP_CIPHER_CTX because
// there are no EVP_CIPHER_CTX_create and EVP_CIPHER_CTX_destroy
// functions.
class ScopedEVPCipherCtx {
 public:
  ScopedEVPCipherCtx();
  ~ScopedEVPCipherCtx();

  EVP_CIPHER_CTX* get() const;

 private:
  EVP_CIPHER_CTX* const ctx_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_SCOPED_EVP_CIPHER_CTX_H_
