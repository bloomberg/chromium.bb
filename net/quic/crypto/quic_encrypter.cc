// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_encrypter.h"

#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/null_encrypter.h"

namespace net {

// static
QuicEncrypter* QuicEncrypter::Create(QuicTag algorithm) {
  switch (algorithm) {
    case kAESG:
      return new Aes128Gcm12Encrypter();
    case kNULL:
      return new NullEncrypter();
    default:
      LOG(FATAL) << "Unsupported algorithm: " << algorithm;
      return NULL;
  }
}

}  // namespace net
