// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "base/string_piece.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"

using base::StringPiece;
using std::string;

namespace net {

void CryptoUtils::GenerateNonce(const QuicClock* clock,
                                QuicRandom* random_generator,
                                string* nonce) {
  // a 4-byte timestamp + 28 random bytes.
  nonce->reserve(kNonceSize);
  nonce->resize(kNonceSize);
  QuicTime::Delta now = clock->NowAsDeltaSinceUnixEpoch();
  uint32 gmt_unix_time = now.ToSeconds();
  const size_t time_size = sizeof(gmt_unix_time);
  memcpy(&(*nonce)[0], &gmt_unix_time, time_size);
  random_generator->RandBytes(&(*nonce)[time_size], kNonceSize - time_size);
}

void CryptoUtils::FillClientHelloMessage(const QuicClientCryptoConfig& config,
                                         const string& nonce,
                                         const string& server_hostname,
                                         CryptoHandshakeMessage* message) {
  message->tag = kCHLO;

  StringPiece value;

  // Version.
  value.set(&config.version, sizeof(config.version));
  message->tag_value_map[kVERS] = value.as_string();

  // Key exchange methods.
  value.set(&config.key_exchange[0],
            config.key_exchange.size() * sizeof(config.key_exchange[0]));
  message->tag_value_map[kKEXS] = value.as_string();

  // Authenticated encryption algorithms.
  value.set(&config.aead[0], config.aead.size() * sizeof(config.aead[0]));
  message->tag_value_map[kAEAD] = value.as_string();

  // Congestion control feedback types.
  value.set(&config.congestion_control[0],
            config.congestion_control.size() *
            sizeof(config.congestion_control[0]));
  message->tag_value_map[kCGST] = value.as_string();

  // Idle connection state lifetime.
  uint32 idle_connection_state_lifetime_secs =
      config.idle_connection_state_lifetime.ToSeconds();
  value.set(&idle_connection_state_lifetime_secs,
            sizeof(idle_connection_state_lifetime_secs));
  message->tag_value_map[kICSL] = value.as_string();

  // Keepalive timeout.
  uint32 keepalive_timeout_secs = config.keepalive_timeout.ToSeconds();
  value.set(&keepalive_timeout_secs, sizeof(keepalive_timeout_secs));
  message->tag_value_map[kKATO] = value.as_string();

  // Connection nonce.
  message->tag_value_map[kNONC] = nonce;

  // Server name indication.
  // If server_hostname is not an IP address literal, it is a DNS hostname.
  IPAddressNumber ip_number;
  if (!server_hostname.empty() &&
      !ParseIPLiteralToNumber(server_hostname, &ip_number)) {
    message->tag_value_map[kSNI] = server_hostname;
  }
}

}  // namespace net
