// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/crypto/quic_decrypter.h"

#include "crypto/hkdf.h"
#include "net/quic/core/crypto/aes_128_gcm_12_decrypter.h"
#include "net/quic/core/crypto/aes_128_gcm_decrypter.h"
#include "net/quic/core/crypto/aes_256_gcm_decrypter.h"
#include "net/quic/core/crypto/chacha20_poly1305_decrypter.h"
#include "net/quic/core/crypto/chacha20_poly1305_tls_decrypter.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/crypto/null_decrypter.h"
#include "net/quic/platform/api/quic_bug_tracker.h"
#include "net/quic/platform/api/quic_logging.h"
#include "third_party/boringssl/src/include/openssl/tls1.h"

namespace net {

// static
std::unique_ptr<QuicDecrypter> QuicDecrypter::Create(QuicTag algorithm) {
  switch (algorithm) {
    case kAESG:
      return std::make_unique<Aes128Gcm12Decrypter>();
    case kCC20:
      return std::make_unique<ChaCha20Poly1305Decrypter>();
    default:
      QUIC_LOG(FATAL) << "Unsupported algorithm: " << algorithm;
      return nullptr;
  }
}

// static
QuicDecrypter* QuicDecrypter::CreateFromCipherSuite(uint32_t cipher_suite) {
  QuicDecrypter* decrypter;
  switch (cipher_suite) {
    case TLS1_CK_AES_128_GCM_SHA256:
      decrypter = new Aes128GcmDecrypter();
      break;
    case TLS1_CK_AES_256_GCM_SHA384:
      decrypter = new Aes256GcmDecrypter();
      break;
    case TLS1_CK_CHACHA20_POLY1305_SHA256:
      decrypter = new ChaCha20Poly1305TlsDecrypter();
      break;
    default:
      QUIC_BUG << "TLS cipher suite is unknown to QUIC";
      return nullptr;
  }
  return decrypter;
}

// static
void QuicDecrypter::DiversifyPreliminaryKey(QuicStringPiece preliminary_key,
                                            QuicStringPiece nonce_prefix,
                                            const DiversificationNonce& nonce,
                                            size_t key_size,
                                            size_t nonce_prefix_size,
                                            std::string* out_key,
                                            std::string* out_nonce_prefix) {
  crypto::HKDF hkdf(preliminary_key.as_string() + nonce_prefix.as_string(),
                    QuicStringPiece(nonce.data(), nonce.size()),
                    "QUIC key diversification", 0, key_size, 0,
                    nonce_prefix_size, 0);
  *out_key = hkdf.server_write_key().as_string();
  *out_nonce_prefix = hkdf.server_write_iv().as_string();
}

}  // namespace net
