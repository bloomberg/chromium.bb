// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONFIG_H_
#define NET_QUIC_QUIC_CONFIG_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_time.h"

namespace net {

// QuicNegotiatedParameters contains non-crypto parameters that are agreed upon
// during the crypto handshake.
class NET_EXPORT_PRIVATE QuicNegotiatedParameters {
 public:
  QuicNegotiatedParameters();

  CryptoTag congestion_control;
  QuicTime::Delta idle_connection_state_lifetime;
  QuicTime::Delta keepalive_timeout;
};

// QuicConfig contains non-crypto configuration options that are negotiated in
// the crypto handshake.
class NET_EXPORT_PRIVATE QuicConfig {
 public:
  QuicConfig();
  ~QuicConfig();

  // SetDefaults sets the members to sensible, default values.
  void SetDefaults();

  // SetFromMessage extracts the non-crypto configuration from |msg| and sets
  // the members of this object to match. This is expected to be called in the
  // case of a server which is loading a server config. The server config
  // contains the non-crypto parameters and so the server will need to keep its
  // QuicConfig in sync with the server config that it'll be sending to
  // clients.
  bool SetFromHandshakeMessage(const CryptoHandshakeMessage& scfg);

  // ToHandshakeMessage serializes the settings in this object as a series of
  // tags /value pairs and adds them to |out|.
  void ToHandshakeMessage(CryptoHandshakeMessage* out) const;

  QuicErrorCode ProcessFinalPeerHandshake(
      const CryptoHandshakeMessage& peer_handshake,
      CryptoUtils::Priority priority,
      QuicNegotiatedParameters* out_params,
      std::string* error_details) const;

 private:
  // Congestion control feedback type.
  CryptoTagVector congestion_control_;
  // Idle connection state lifetime
  QuicTime::Delta idle_connection_state_lifetime_;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicTime::Delta keepalive_timeout_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONFIG_H_
