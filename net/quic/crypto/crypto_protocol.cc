// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_protocol.h"

namespace net {

CryptoHandshakeMessage::CryptoHandshakeMessage() {}
CryptoHandshakeMessage::~CryptoHandshakeMessage() {}

QuicCryptoConfig::QuicCryptoConfig()
    : version(0),
      idle_connection_state_lifetime(QuicTime::Delta::Zero()),
      keepalive_timeout(QuicTime::Delta::Zero()) {
}

QuicCryptoConfig::~QuicCryptoConfig() {}

void QuicCryptoConfig::SetClientDefaults() {
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
  // TODO(wtc): add kINAR when inter-arrival is supported.
  congestion_control.resize(1);
  congestion_control[0] = kQBIC;

  // Idle connection state lifetime.
  idle_connection_state_lifetime = QuicTime::Delta::FromSeconds(300);

  // Keepalive timeout.
  keepalive_timeout = QuicTime::Delta::Zero();  // Don't send keepalive probes.
}

void QuicCryptoConfig::SetServerDefaults() {
  // Version must be 0.
  version = 0;

  // Key exchange methods.
  // Add only NIST curve P-256 for now to ensure it is selected.
  key_exchange.resize(1);
  key_exchange[0] = kP256;

  // Authenticated encryption algorithms.
  // Add only AES-GCM for now to ensure it is selected.
  aead.resize(1);
  aead[0] = kAESG;

  // Congestion control feedback types.
  // TODO(wtc): add kINAR when inter-arrival is supported.
  congestion_control.resize(1);
  congestion_control[0] = kQBIC;

  // Idle connection state lifetime.
  idle_connection_state_lifetime = QuicTime::Delta::FromSeconds(300);

  // Keepalive timeout.
  keepalive_timeout = QuicTime::Delta::Zero();  // Don't send keepalive probes.
}

QuicCryptoNegotiatedParams::QuicCryptoNegotiatedParams()
    : version(0),
      key_exchange(0),
      aead(0),
      congestion_control(0),
      idle_connection_state_lifetime(QuicTime::Delta::Zero()) {
}

QuicCryptoNegotiatedParams::~QuicCryptoNegotiatedParams() {}

void QuicCryptoNegotiatedParams::SetDefaults() {
  // TODO(wtc): actually negotiate the parameters using client defaults
  // and server defaults.

  // Version must be 0.
  version = 0;

  // Key exchange method.
  key_exchange = kP256;

  // Authenticated encryption algorithm.
  aead = kAESG;

  // Congestion control feedback type.
  congestion_control = kQBIC;

  // Idle connection state lifetime.
  idle_connection_state_lifetime = QuicTime::Delta::FromSeconds(300);
}

}  // namespace net
