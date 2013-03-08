// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
#define NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_

#include <map>
#include <string>

#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"

namespace net {

class KeyExchange;
class QuicRandom;
class QuicClock;

// TODO(rch): sync with server more rationally
class NET_EXPORT_PRIVATE QuicServerConfigProtobuf {
 public:
  class NET_EXPORT_PRIVATE PrivateKey {
   public:
    CryptoTag tag() const {
      return tag_;
    }
    void set_tag(CryptoTag tag) {
      tag_ = tag;
    }
    std::string private_key() const {
      return private_key_;
    }
    void set_private_key(std::string key) {
      private_key_ = key;
    }

   private:
    CryptoTag tag_;
    std::string private_key_;
  };

  QuicServerConfigProtobuf();
  ~QuicServerConfigProtobuf();

  size_t key_size() const {
    return keys_.size();
  }

  const PrivateKey& key(size_t i) const {
    DCHECK_GT(keys_.size(), i);
    return *keys_[i];
  }

  std::string config() const {
    return config_;
  }

  void set_config(base::StringPiece config) {
    config_ = config.as_string();
  }

  QuicServerConfigProtobuf::PrivateKey* add_key() {
    keys_.push_back(new PrivateKey);
    return keys_.back();
  }

 private:
  std::vector<PrivateKey*> keys_;
  std::string config_;
};

// Parameters negotiated by the crypto handshake.
struct NET_EXPORT_PRIVATE QuicCryptoNegotiatedParams {
  // Initializes the members to 0 or empty values.
  QuicCryptoNegotiatedParams();
  ~QuicCryptoNegotiatedParams();

  uint16 version;
  CryptoTag key_exchange;
  CryptoTag aead;
  std::string premaster_secret;
};

// QuicCryptoConfig contains common configuration between clients and servers.
class NET_EXPORT_PRIVATE QuicCryptoConfig {
 public:
  QuicCryptoConfig();
  ~QuicCryptoConfig();

  // ProcessPeerHandshake performs common processing when receiving a peer's
  // handshake message.
  bool ProcessPeerHandshake(const CryptoHandshakeMessage& peer_handshake,
                            CryptoUtils::Priority priority,
                            QuicCryptoNegotiatedParams* out_params,
                            std::string* error_details) const;

  // Protocol version
  uint16 version;
  // Key exchange methods. The following two members' values correspond by
  // index.
  CryptoTagVector kexs;
  std::vector<KeyExchange*> key_exchanges;
  // Authenticated encryption with associated data (AEAD) algorithms
  CryptoTagVector aead;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoConfig);
};

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client.
class NET_EXPORT_PRIVATE QuicCryptoClientConfig : public QuicCryptoConfig {
 public:
  // Sets the members to reasonable, default values. |rand| is used in order to
  // generate ephemeral ECDH keys.
  void SetDefaults(QuicRandom* rand);

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object.
  void FillClientHello(const std::string& nonce,
                       const std::string& server_hostname,
                       CryptoHandshakeMessage* out);

  // ProcessServerHello processes the tags in |server_hello|, writes the
  // negotiated parameters to |out_params| and returns QUIC_NO_ERROR. If
  // |server_hello| is unacceptable then it puts an error message in
  // |error_details| and returns an error code.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   QuicCryptoNegotiatedParams* out_params,
                                   std::string* error_details);
};

// QuicCryptoServerConfig contains the crypto configuration of a QUIC server.
// Unlike a client, a QUIC server can have multiple configurations active in
// order to support clients resuming with a previous configuration.
// TODO(agl): when adding configurations at runtime is added, this object will
// need to consider locking.
class NET_EXPORT_PRIVATE QuicCryptoServerConfig {
 public:
  QuicCryptoServerConfig();
  ~QuicCryptoServerConfig();

  // ConfigForTesting generates a QuicServerConfigProtobuf protobuf suitable
  // for using in tests. |extra_tags| contains additional key/value pairs that
  // will be inserted into the config.
  static QuicServerConfigProtobuf* ConfigForTesting(
      QuicRandom* rand,
      const QuicClock* clock,
      const CryptoHandshakeMessage& extra_tags);

  // It returns the tag/value map of the config if successful. The caller takes
  // ownership of the map.
  CryptoTagValueMap* AddConfig(QuicServerConfigProtobuf* protobuf);

  // AddTestingConfig creates a test config and then calls AddConfig to add it.
  // Any tags in |extra_tags| will be copied into the config.
  CryptoTagValueMap* AddTestingConfig(QuicRandom* rand,
                                      const QuicClock* clock,
                                      const CryptoHandshakeMessage& extra_tags);

  // ProcessClientHello processes |client_hello| and decides whether to accept
  // or reject the connection. If the connection is to be accepted, |out| is
  // set to the contents of the ServerHello, |out_params| is completed and true
  // is returned. |nonce| is used as the server's nonce.  Otherwise |out| is
  // set to be a REJ message and false is returned.
  bool ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                          const std::string& nonce,
                          CryptoHandshakeMessage* out,
                          QuicCryptoNegotiatedParams* out_params,
                          std::string* error_details);

 private:
  // Config represents a server config: a collection of preferences and
  // Diffie-Hellman public values.
  struct Config : public QuicCryptoConfig {
    Config();
    ~Config();

    // serialized contains the bytes of this server config, suitable for sending
    // on the wire.
    std::string serialized;

    // tag_value_map contains the raw key/value pairs for the config.
    CryptoTagValueMap tag_value_map;
  };

  std::map<ServerConfigID, Config*> configs_;

  std::string active_config_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
