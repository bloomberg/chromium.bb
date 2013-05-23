// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/scoped_evp_cipher_ctx.h"

#include <openssl/evp.h>

namespace net {

ScopedEVPCipherCtx::ScopedEVPCipherCtx()
    : ctx_(EVP_CIPHER_CTX_new()) { }

ScopedEVPCipherCtx::~ScopedEVPCipherCtx() {
  EVP_CIPHER_CTX_free(ctx_);
}

EVP_CIPHER_CTX* ScopedEVPCipherCtx::get() const {
  return ctx_;
}

}  // namespace net
