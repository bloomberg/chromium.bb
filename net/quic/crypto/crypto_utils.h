// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic crypto

#ifndef NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
#define NET_QUIC_CRYPTO_CRYPTO_UTILS_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

class QuicClock;
class QuicRandom;
struct CryptoHandshakeMessage;
struct QuicClientCryptoConfig;

class NET_EXPORT_PRIVATE CryptoUtils {
 public:
  // Generates the connection nonce.
  static void GenerateNonce(const QuicClock* clock,
                            QuicRandom* random_generator,
                            std::string* nonce);

  static void FillClientHelloMessage(const QuicClientCryptoConfig& config,
                                     const std::string& nonce,
                                     CryptoHandshakeMessage* message);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
