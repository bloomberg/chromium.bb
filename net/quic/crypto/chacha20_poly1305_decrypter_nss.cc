// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/chacha20_poly1305_decrypter.h"

#include <pk11pub.h>

#include "base/logging.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 32;
const size_t kNoncePrefixSize = 0;

}  // namespace

#if defined(USE_NSS)

// System NSS doesn't support ChaCha20+Poly1305 yet.

ChaCha20Poly1305Decrypter::ChaCha20Poly1305Decrypter()
    : AeadBaseDecrypter(CKM_INVALID_MECHANISM, NULL, kKeySize,
                        kAuthTagSize, kNoncePrefixSize) {
  NOTIMPLEMENTED();
}

ChaCha20Poly1305Decrypter::~ChaCha20Poly1305Decrypter() {}

// static
bool ChaCha20Poly1305Decrypter::IsSupported() {
  return false;
}

void ChaCha20Poly1305Decrypter::FillAeadParams(StringPiece nonce,
                                               StringPiece associated_data,
                                               size_t auth_tag_size,
                                               AeadParams* aead_params) const {
  NOTIMPLEMENTED();
}

#else  // defined(USE_NSS)

ChaCha20Poly1305Decrypter::ChaCha20Poly1305Decrypter()
    : AeadBaseDecrypter(CKM_NSS_CHACHA20_POLY1305, PK11_Decrypt, kKeySize,
                        kAuthTagSize, kNoncePrefixSize) {
  COMPILE_ASSERT(kKeySize <= kMaxKeySize, key_size_too_big);
  COMPILE_ASSERT(kNoncePrefixSize <= kMaxNoncePrefixSize,
                 nonce_prefix_size_too_big);
}

ChaCha20Poly1305Decrypter::~ChaCha20Poly1305Decrypter() {}

// static
bool ChaCha20Poly1305Decrypter::IsSupported() {
  // TODO(wtc): return true when FillAeadParams is implemented.
  return false;
}

void ChaCha20Poly1305Decrypter::FillAeadParams(StringPiece nonce,
                                               StringPiece associated_data,
                                               size_t auth_tag_size,
                                               AeadParams* aead_params) const {
  // TODO(wtc): implement this.
  NOTIMPLEMENTED();
}

#endif  // defined(USE_NSS)

}  // namespace net
