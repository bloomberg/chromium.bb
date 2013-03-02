// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
#define NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_

#include <map>
#include <string>

#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"

namespace net {

class KeyExchange;
class QuicRandom;
class QuicClock;

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client.
class NET_EXPORT_PRIVATE QuicCryptoClientConfig {
 public:
  // Initializes the members to 0 or empty values.
  QuicCryptoClientConfig();
  ~QuicCryptoClientConfig();

  // Sets the members to reasonable, default values.
  void SetDefaults();

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object.
  void FillClientHello(const std::string& nonce,
                       const std::string& server_hostname,
                       CryptoHandshakeMessage* out);

  // Protocol version
  uint16 version;
  // Key exchange methods
  CryptoTagVector key_exchange;
  // Authenticated encryption with associated data (AEAD) algorithms
  CryptoTagVector aead;
  // Congestion control feedback types
  CryptoTagVector congestion_control;
  // Idle connection state lifetime
  QuicTime::Delta idle_connection_state_lifetime;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicTime::Delta keepalive_timeout;
};

// TODO(rtenneti): Delete QuicCryptoServerConfig.
//
// QuicCryptoServerConfig contains the crypto configuration of a QUIC server.
// Unlike a client, a QUIC server can have multiple configurations active in
// order to support clients resuming with a previous configuration.
// TODO(agl): when adding configurations at runtime is added, this object will
// need to consider locking.
class NET_EXPORT_PRIVATE QuicCryptoServerConfig {
 public:
  QuicCryptoServerConfig();
  ~QuicCryptoServerConfig();

  // AddTestingConfig adds a single, testing config.
  void AddTestingConfig(QuicRandom* rand, const QuicClock* clock);

  // ProcessClientHello processes |client_hello| and decides whether to accept
  // or reject the connection. If the connection is to be accepted, |out| is
  // set to the contents of the ServerHello and true is returned. |nonce| is
  // used as the server's nonce.  Otherwise |out| is set to be a REJ message
  // and false is returned.
  bool ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                          const std::string& nonce,
                          CryptoHandshakeMessage* out);

 private:
  // Config represents a server config: a collection of preferences and
  // Diffie-Hellman public values.
  struct Config {
    Config();
    ~Config();

    // serialized contains the bytes of this server config, suitable for sending
    // on the wire.
    std::string serialized;
    // key_exchange_tags contains the key exchange methods from the config,
    // in preference order.
    CryptoTagVector key_exchange_tags;
    // key_exchanges maps from elements of |key_exchange_tags| to the object
    // that implements the specific key exchange.
    std::map<CryptoTag, KeyExchange*> key_exchanges;
  };

  std::map<ServerConfigID, Config*> configs_;

  std::string active_config_;
};

// Parameters negotiated by the crypto handshake.
struct NET_EXPORT_PRIVATE QuicCryptoNegotiatedParams {
  // Initializes the members to 0 or empty values.
  QuicCryptoNegotiatedParams();
  ~QuicCryptoNegotiatedParams();

  // Sets the members to the values that would be negotiated from the default
  // client-side and server-side configuration settings.
  void SetDefaults();

  uint16 version;
  CryptoTag key_exchange;
  CryptoTag aead;
  CryptoTag congestion_control;
  QuicTime::Delta idle_connection_state_lifetime;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
