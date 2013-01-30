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

void CryptoUtils::FillClientHelloMessage(
    const QuicCryptoConfig& client_config,
    const string& nonce,
    const string& server_hostname,
    CryptoHandshakeMessage* message) {
  message->tag = kCHLO;

  // Version.
  message->tag_value_map[kVERS] = EncodeSingleValue(client_config.version);

  // Key exchange methods.
  message->tag_value_map[kKEXS] = EncodeVectorValue(client_config.key_exchange);

  // Authenticated encryption algorithms.
  message->tag_value_map[kAEAD] = EncodeVectorValue(client_config.aead);

  // Congestion control feedback types.
  message->tag_value_map[kCGST] =
      EncodeVectorValue(client_config.congestion_control);

  // Idle connection state lifetime.
  uint32 idle_connection_state_lifetime_secs =
      client_config.idle_connection_state_lifetime.ToSeconds();
  message->tag_value_map[kICSL] =
      EncodeSingleValue(idle_connection_state_lifetime_secs);

  // Keepalive timeout.
  uint32 keepalive_timeout_secs = client_config.keepalive_timeout.ToSeconds();
  message->tag_value_map[kKATO] = EncodeSingleValue(keepalive_timeout_secs);

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

void CryptoUtils::FillServerHelloMessage(
    const QuicCryptoNegotiatedParams& negotiated_params,
    const string& nonce,
    CryptoHandshakeMessage* message) {
  message->tag = kSHLO;

  // Version.
  message->tag_value_map[kVERS] = EncodeSingleValue(negotiated_params.version);

  // Key exchange method.
  message->tag_value_map[kKEXS] =
      EncodeSingleValue(negotiated_params.key_exchange);

  // Authenticated encryption algorithm.
  message->tag_value_map[kAEAD] = EncodeSingleValue(negotiated_params.aead);

  // Congestion control feedback type.
  message->tag_value_map[kCGST] =
      EncodeSingleValue(negotiated_params.congestion_control);

  // Idle connection state lifetime.
  uint32 idle_connection_state_lifetime_secs =
      negotiated_params.idle_connection_state_lifetime.ToSeconds();
  message->tag_value_map[kICSL] =
      EncodeSingleValue(idle_connection_state_lifetime_secs);

  // Keepalive timeout?

  // Connection nonce.
  message->tag_value_map[kNONC] = nonce;
}

}  // namespace net
