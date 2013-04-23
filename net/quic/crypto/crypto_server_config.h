// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_H_
#define NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class KeyExchange;
class ProofSource;
class QuicClock;
class QuicDecrypter;
class QuicEncrypter;
class QuicRandom;
class QuicServerConfigProtobuf;
class StrikeRegister;

namespace test {
class QuicCryptoServerConfigPeer;
}  // namespace test

// QuicCryptoServerConfig contains the crypto configuration of a QUIC server.
// Unlike a client, a QUIC server can have multiple configurations active in
// order to support clients resuming with a previous configuration.
// TODO(agl): when adding configurations at runtime is added, this object will
// need to consider locking.
class NET_EXPORT_PRIVATE QuicCryptoServerConfig {
 public:
  // |source_address_token_secret|: secret key material used for encrypting and
  //     decrypting source address tokens. It can be of any length as it is fed
  //     into a KDF before use. In tests, use TESTING.
  explicit QuicCryptoServerConfig(
      base::StringPiece source_address_token_secret);
  ~QuicCryptoServerConfig();

  // TESTING is a magic parameter for passing to the constructor in tests.
  static const char TESTING[];

  // DefaultConfig generates a QuicServerConfigProtobuf protobuf suitable
  // for using in tests. |extra_tags| contains additional key/value pairs that
  // will be inserted into the config.
  static QuicServerConfigProtobuf* DefaultConfig(
      QuicRandom* rand,
      const QuicClock* clock,
      const CryptoHandshakeMessage& extra_tags);

  // AddConfig adds a QuicServerConfigProtobuf to the availible configurations.
  // It returns the SCFG message from the config if successful. The caller
  // takes ownership of the CryptoHandshakeMessage.
  CryptoHandshakeMessage* AddConfig(QuicServerConfigProtobuf* protobuf);

  // AddDefaultConfig creates a config and then calls AddConfig to
  // add it. Any tags in |extra_tags| will be copied into the config.
  CryptoHandshakeMessage* AddDefaultConfig(
      QuicRandom* rand,
      const QuicClock* clock,
      const CryptoHandshakeMessage& extra_tags);

  // ProcessClientHello processes |client_hello| and decides whether to accept
  // or reject the connection. If the connection is to be accepted, |out| is
  // set to the contents of the ServerHello, |out_params| is completed and
  // QUIC_NO_ERROR is returned. Otherwise |out| is set to be a REJ message and
  // an error code is returned.
  //
  // client_hello: the incoming client hello message.
  // guid: the GUID for the connection, which is used in key derivation.
  // client_ip: the IP address of the client, which is used to generate and
  //     validate source-address tokens.
  // now_since_epoch: the current time, as a delta since the unix epoch,
  //     which is used to validate client nonces.
  // rand: an entropy source
  // params: the state of the handshake. This may be updated with a server
  //     nonce when we send a rejection. After a successful handshake, this will
  //     contain the state of the connection.
  // out: the resulting handshake message (either REJ or SHLO)
  // error_details: used to store a string describing any error.
  QuicErrorCode ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                                   QuicGuid guid,
                                   const IPEndPoint& client_ip,
                                   QuicTime::Delta now_since_epoch,
                                   QuicRandom* rand,
                                   QuicCryptoNegotiatedParameters* params,
                                   CryptoHandshakeMessage* out,
                                   std::string* error_details) const;

  // SetProofSource installs |proof_source| as the ProofSource for handshakes.
  // This object takes ownership of |proof_source|.
  void SetProofSource(ProofSource* proof_source);

 private:
  friend class test::QuicCryptoServerConfigPeer;

  // Config represents a server config: a collection of preferences and
  // Diffie-Hellman public values.
  struct Config : public QuicCryptoConfig {
    Config();
    ~Config();

    // serialized contains the bytes of this server config, suitable for sending
    // on the wire.
    std::string serialized;
    // id contains the SCID of this server config.
    std::string id;
    // orbit contains the orbit value for this config: an opaque identifier
    // used to identify clusters of server frontends.
    unsigned char orbit[kOrbitSize];

    // key_exchanges contains key exchange objects with the private keys
    // already loaded. The values correspond, one-to-one, with the tags in
    // |kexs| from the parent class.
    std::vector<KeyExchange*> key_exchanges;

    // tag_value_map contains the raw key/value pairs for the config.
    CryptoTagValueMap tag_value_map;

   private:
    DISALLOW_COPY_AND_ASSIGN(Config);
  };

  // NewSourceAddressToken returns a fresh source address token for the given
  // IP address.
  std::string NewSourceAddressToken(const IPEndPoint& ip,
                                    QuicRandom* rand,
                                    QuicTime::Delta now_since_epoch) const;

  // ValidateSourceAddressToken returns true if the source address token in
  // |token| is a valid and timely token for the IP address |ip| given that the
  // current time is |now|.
  bool ValidateSourceAddressToken(base::StringPiece token,
                                  const IPEndPoint& ip,
                                  QuicTime::Delta now_since_epoch) const;

  std::map<ServerConfigID, Config*> configs_;

  ServerConfigID active_config_;

  mutable base::Lock strike_register_lock_;
  // strike_register_ contains a data structure that keeps track of previously
  // observed client nonces in order to prevent replay attacks.
  mutable scoped_ptr<StrikeRegister> strike_register_;

  // These members are used to encrypt and decrypt the source address tokens
  // that we receive from and send to clients.
  scoped_ptr<QuicEncrypter> source_address_token_encrypter_;
  scoped_ptr<QuicDecrypter> source_address_token_decrypter_;

  // proof_source_ contains an object that can provide certificate chains and
  // signatures.
  scoped_ptr<ProofSource> proof_source_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_H_
