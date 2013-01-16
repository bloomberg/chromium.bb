// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_protocol.h"

namespace net {

CryptoHandshakeMessage::CryptoHandshakeMessage() {}
CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

QuicClientCryptoConfig::QuicClientCryptoConfig() : version(0) {
}

QuicClientCryptoConfig::~QuicClientCryptoConfig() {}

void QuicClientCryptoConfig::SetDefaults() {
  // Version must be 0.
  version = 0;

  // Key exchange methods.
  key_exchange.resize(2);
  key_exchange[0] = kC255;
  key_exchange[1] = kP256;

  // Authenticated encryption algorithms.
  aead.resize(2);
  aead[0] = kAESG;
  aead[1] = kAESH;

  // Congestion control feedback types.
  congestion_control.resize(1);
  congestion_control[0] = kQBIC;

  // Idle connection state lifetime.
  idle_connection_state_lifetime = QuicTime::Delta::FromMilliseconds(300000);

  // Keepalive timeout.
  keepalive_timeout = QuicTime::Delta();  // Don't send keepalive probes.
}

}  // namespace net
