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

class EphemeralKeySource;
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
  enum {
    // kDefaultExpiry can be passed to DefaultConfig to select the default
    // expiry time.
    kDefaultExpiry = 0,
  };

  // |source_address_token_secret|: secret key material used for encrypting and
  //     decrypting source address tokens. It can be of any length as it is fed
  //     into a KDF before use. In tests, use TESTING.
  explicit QuicCryptoServerConfig(
      base::StringPiece source_address_token_secret);
  ~QuicCryptoServerConfig();

  // TESTING is a magic parameter for passing to the constructor in tests.
  static const char TESTING[];

  // DefaultConfig generates a QuicServerConfigProtobuf protobuf suitable for
  // using in tests. If |expiry_time| is non-zero then it's used as the expiry
  // for the server config in UNIX epoch seconds. Otherwise the default expiry
  // time is six months from now.
  static QuicServerConfigProtobuf* DefaultConfig(
      QuicRandom* rand,
      const QuicClock* clock,
      uint64 expiry_time);

  // AddConfig adds a QuicServerConfigProtobuf to the availible configurations.
  // It returns the SCFG message from the config if successful. The caller
  // takes ownership of the CryptoHandshakeMessage.
  CryptoHandshakeMessage* AddConfig(QuicServerConfigProtobuf* protobuf);

  // AddDefaultConfig calls DefaultConfig to create a config and then calls
  // AddConfig to add it. See the comment for |DefaultConfig| for details of
  // the arguments.
  CryptoHandshakeMessage* AddDefaultConfig(
      QuicRandom* rand,
      const QuicClock* clock,
      uint64 expiry_time);

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
  // clock: used to validate client nonces and ephemeral keys.
  // rand: an entropy source
  // params: the state of the handshake. This may be updated with a server
  //     nonce when we send a rejection. After a successful handshake, this will
  //     contain the state of the connection.
  // out: the resulting handshake message (either REJ or SHLO)
  // error_details: used to store a string describing any error.
  QuicErrorCode ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                                   QuicGuid guid,
                                   const IPEndPoint& client_ip,
                                   const QuicClock* clock,
                                   QuicRandom* rand,
                                   QuicCryptoNegotiatedParameters* params,
                                   CryptoHandshakeMessage* out,
                                   std::string* error_details) const;

  // SetProofSource installs |proof_source| as the ProofSource for handshakes.
  // This object takes ownership of |proof_source|.
  void SetProofSource(ProofSource* proof_source);

  // SetEphemeralKeySource installs an object that can cache ephemeral keys for
  // a short period of time. This object takes ownership of
  // |ephemeral_key_source|. If not set then ephemeral keys will be generated
  // per-connection.
  void SetEphemeralKeySource(EphemeralKeySource* ephemeral_key_source);

  // set_strike_register_max_entries sets the maximum number of entries that
  // the internal strike register will hold. If the strike register fills up
  // then the oldest entries (by the client's clock) will be dropped.
  void set_strike_register_max_entries(uint32 max_entries);

  // set_strike_register_window_secs sets the number of seconds around the
  // current time that the strike register will attempt to be authoritative
  // for. Setting a larger value allows for greater client clock-skew, but
  // means that the quiescent startup period must be longer.
  void set_strike_register_window_secs(uint32 window_secs);

  // set_source_address_token_future_secs sets the number of seconds into the
  // future that source-address tokens will be accepted from. Since
  // source-address tokens are authenticated, this should only happen if
  // another, valid server has clock-skew.
  void set_source_address_token_future_secs(uint32 future_secs);

  // set_source_address_token_lifetime_secs sets the number of seconds that a
  // source-address token will be valid for.
  void set_source_address_token_lifetime_secs(uint32 lifetime_secs);

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
    QuicTagValueMap tag_value_map;

   private:
    DISALLOW_COPY_AND_ASSIGN(Config);
  };

  // NewSourceAddressToken returns a fresh source address token for the given
  // IP address.
  std::string NewSourceAddressToken(const IPEndPoint& ip,
                                    QuicRandom* rand,
                                    QuicWallTime now) const;

  // ValidateSourceAddressToken returns true if the source address token in
  // |token| is a valid and timely token for the IP address |ip| given that the
  // current time is |now|.
  bool ValidateSourceAddressToken(base::StringPiece token,
                                  const IPEndPoint& ip,
                                  QuicWallTime now) const;

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

  // ephemeral_key_source_ contains an object that caches ephemeral keys for a
  // short period of time.
  scoped_ptr<EphemeralKeySource> ephemeral_key_source_;

  // These fields store configuration values. See the comments for their
  // respective setter functions.
  uint32 strike_register_max_entries_;
  uint32 strike_register_window_secs_;
  uint32 source_address_token_future_secs_;
  uint32 source_address_token_lifetime_secs_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_H_
