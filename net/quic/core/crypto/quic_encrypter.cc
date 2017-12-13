// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/crypto/quic_encrypter.h"

#include "net/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/core/crypto/aes_128_gcm_encrypter.h"
#include "net/quic/core/crypto/aes_256_gcm_encrypter.h"
#include "net/quic/core/crypto/chacha20_poly1305_encrypter.h"
#include "net/quic/core/crypto/chacha20_poly1305_tls_encrypter.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/crypto/null_encrypter.h"
#include "net/quic/platform/api/quic_bug_tracker.h"
#include "net/quic/platform/api/quic_logging.h"
#include "third_party/boringssl/src/include/openssl/tls1.h"

namespace net {

// static
std::unique_ptr<QuicEncrypter> QuicEncrypter::Create(QuicTag algorithm) {
  switch (algorithm) {
    case kAESG:
      return std::make_unique<Aes128Gcm12Encrypter>();
    case kCC20:
      return std::make_unique<ChaCha20Poly1305Encrypter>();
    default:
      QUIC_LOG(FATAL) << "Unsupported algorithm: " << algorithm;
      return nullptr;
  }
}

// static
QuicEncrypter* QuicEncrypter::CreateFromCipherSuite(uint32_t cipher_suite) {
  QuicEncrypter* encrypter;
  switch (cipher_suite) {
    case TLS1_CK_AES_128_GCM_SHA256:
      encrypter = new Aes128GcmEncrypter();
      break;
    case TLS1_CK_AES_256_GCM_SHA384:
      encrypter = new Aes256GcmEncrypter();
      break;
    case TLS1_CK_CHACHA20_POLY1305_SHA256:
      encrypter = new ChaCha20Poly1305TlsEncrypter();
      break;
    default:
      QUIC_BUG << "TLS cipher suite is unknown to QUIC";
      return nullptr;
  }
  return encrypter;
}

}  // namespace net
