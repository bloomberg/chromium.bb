// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_handshake.h"

#include "base/stl_util.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/key_exchange.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_protocol.h"

using std::string;

namespace net {

QuicCryptoClientConfig::QuicCryptoClientConfig()
    : version(0),
      idle_connection_state_lifetime(QuicTime::Delta::Zero()),
      keepalive_timeout(QuicTime::Delta::Zero()) {
}

QuicCryptoClientConfig::~QuicCryptoClientConfig() {}

void QuicCryptoClientConfig::SetDefaults() {
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

void QuicCryptoClientConfig::FillClientHello(const string& nonce,
                                             const string& server_hostname,
                                             CryptoHandshakeMessage* out) {
  out->tag = kCHLO;

  out->SetValue(kVERS, version);
  out->SetVector(kKEXS, key_exchange);
  out->SetVector(kAEAD, aead);
  out->SetVector(kCGST, congestion_control);
  out->tag_value_map[kNONC] = nonce;

  // Idle connection state lifetime.
  uint32 idle_connection_state_lifetime_secs =
      idle_connection_state_lifetime.ToSeconds();
  out->SetValue(kICSL, idle_connection_state_lifetime_secs);

  // Keepalive timeout.
  uint32 keepalive_timeout_secs = keepalive_timeout.ToSeconds();
  out->SetValue(kKATO, keepalive_timeout_secs);

  // Server name indication.
  // If server_hostname is not an IP address literal, it is a DNS hostname.
  IPAddressNumber ip_number;
  if (!server_hostname.empty() &&
      !ParseIPLiteralToNumber(server_hostname, &ip_number)) {
    out->tag_value_map[kSNI] = server_hostname;
  }
}

// TODO(rtenneti): Delete QuicCryptoServerConfig.
QuicCryptoServerConfig::QuicCryptoServerConfig() {
}

QuicCryptoServerConfig::~QuicCryptoServerConfig() {
  STLDeleteValues(&configs_);
}

void QuicCryptoServerConfig::AddTestingConfig(QuicRandom* rand,
                                              const QuicClock* clock) {
}

bool QuicCryptoServerConfig::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    const string& nonce,
    CryptoHandshakeMessage* out) {
  CHECK(!configs_.empty());
  const Config* config(configs_[active_config_]);

  // TODO(agl): This is obviously missing most of the handshake.
  out->tag = kSHLO;
  out->tag_value_map[kNONC] = nonce;
  out->tag_value_map[kSCFG] = config->serialized;
  return true;
}

QuicCryptoServerConfig::Config::Config() {
}

QuicCryptoServerConfig::Config::~Config() {
  STLDeleteValues(&key_exchanges);
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
