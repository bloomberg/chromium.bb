// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
#define NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class KeyExchange;
class QuicClock;
class QuicDecrypter;
class QuicEncrypter;
class QuicRandom;
class QuicServerConfigProtobuf;

namespace test {
class QuicCryptoServerConfigPeer;
}  // namespace test

// An intermediate format of a handshake message that's convenient for a
// CryptoFramer to serialize from or parse into.
class NET_EXPORT_PRIVATE CryptoHandshakeMessage {
 public:
  CryptoHandshakeMessage();
  CryptoHandshakeMessage(const CryptoHandshakeMessage& other);
  ~CryptoHandshakeMessage();

  CryptoHandshakeMessage& operator=(const CryptoHandshakeMessage& other);

  // Clears state.
  void Clear();

  // GetSerialized returns the serialized form of this message and caches the
  // result. Subsequently altering the message does not invalidate the cache.
  const QuicData& GetSerialized() const;

  // SetValue sets an element with the given tag to the raw, memory contents of
  // |v|.
  template<class T> void SetValue(CryptoTag tag, const T& v) {
    tag_value_map_[tag] = std::string(reinterpret_cast<const char*>(&v),
                                      sizeof(v));
  }

  // SetVector sets an element with the given tag to the raw contents of an
  // array of elements in |v|.
  template<class T> void SetVector(CryptoTag tag, const std::vector<T>& v) {
    if (v.empty()) {
      tag_value_map_[tag] = std::string();
    } else {
      tag_value_map_[tag] = std::string(reinterpret_cast<const char*>(&v[0]),
                                        v.size() * sizeof(T));
    }
  }

  // Returns the message tag.
  CryptoTag tag() const { return tag_; }
  // Sets the message tag.
  void set_tag(CryptoTag tag) { tag_ = tag; }

  const CryptoTagValueMap& tag_value_map() const { return tag_value_map_; }

  void Insert(CryptoTagValueMap::const_iterator begin,
              CryptoTagValueMap::const_iterator end);

  // SetTaglist sets an element with the given tag to contain a list of tags,
  // passed as varargs. The argument list must be terminated with a 0 element.
  void SetTaglist(CryptoTag tag, ...);

  void SetStringPiece(CryptoTag tag, base::StringPiece value);

  // GetTaglist finds an element with the given tag containing zero or more
  // tags. If such a tag doesn't exist, it returns false. Otherwise it sets
  // |out_tags| and |out_len| to point to the array of tags and returns true.
  // The array points into the CryptoHandshakeMessage and is valid only for as
  // long as the CryptoHandshakeMessage exists and is not modified.
  QuicErrorCode GetTaglist(CryptoTag tag, const CryptoTag** out_tags,
                           size_t* out_len) const;

  bool GetStringPiece(CryptoTag tag, base::StringPiece* out) const;

  // GetNthValue16 interprets the value with the given tag to be a series of
  // 16-bit length prefixed values and it returns the subvalue with the given
  // index.
  QuicErrorCode GetNthValue16(CryptoTag tag,
                              unsigned index,
                              base::StringPiece* out) const;
  bool GetString(CryptoTag tag, std::string* out) const;
  QuicErrorCode GetUint16(CryptoTag tag, uint16* out) const;
  QuicErrorCode GetUint32(CryptoTag tag, uint32* out) const;

  // DebugString returns a multi-line, string representation of the message
  // suitable for including in debug output.
  std::string DebugString() const;

 private:
  // GetPOD is a utility function for extracting a plain-old-data value. If
  // |tag| exists in the message, and has a value of exactly |len| bytes then
  // it copies |len| bytes of data into |out|. Otherwise |len| bytes at |out|
  // are zeroed out.
  //
  // If used to copy integers then this assumes that the machine is
  // little-endian.
  QuicErrorCode GetPOD(CryptoTag tag, void* out, size_t len) const;

  std::string DebugStringInternal(size_t indent) const;

  CryptoTag tag_;
  CryptoTagValueMap tag_value_map_;

  // The serialized form of the handshake message. This member is constructed
  // lasily.
  mutable scoped_ptr<QuicData> serialized_;
};

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

// TODO(rtenneti): sync with server more rationally.
class NET_EXPORT_PRIVATE SourceAddressToken {
 public:
  SourceAddressToken();
  ~SourceAddressToken();

  std::string SerializeAsString() const;

  bool ParseFromArray(unsigned char* plaintext, size_t plaintext_length);

  std::string ip() const {
    return ip_;
  }

  int64 timestamp() const {
    return timestamp_;
  }

  void set_ip(base::StringPiece ip) {
    ip_ = ip.as_string();
  }

  void set_timestamp(int64 timestamp) {
    timestamp_ = timestamp;
  }

 private:
  std::string ip_;
  int64 timestamp_;
};

// Parameters negotiated by the crypto handshake.
struct NET_EXPORT_PRIVATE QuicCryptoNegotiatedParameters {
  // Initializes the members to 0 or empty values.
  QuicCryptoNegotiatedParameters();
  ~QuicCryptoNegotiatedParameters();

  uint16 version;
  CryptoTag key_exchange;
  CryptoTag aead;
  std::string premaster_secret;
  scoped_ptr<QuicEncrypter> encrypter;
  scoped_ptr<QuicDecrypter> decrypter;
  std::string server_config_id;
};

// QuicCryptoConfig contains common configuration between clients and servers.
class NET_EXPORT_PRIVATE QuicCryptoConfig {
 public:
  QuicCryptoConfig();
  ~QuicCryptoConfig();

  // Protocol version
  uint16 version;
  // Key exchange methods. The following two members' values correspond by
  // index.
  CryptoTagVector kexs;
  // Authenticated encryption with associated data (AEAD) algorithms.
  CryptoTagVector aead;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoConfig);
};

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client.
class NET_EXPORT_PRIVATE QuicCryptoClientConfig : public QuicCryptoConfig {
 public:
  // A CachedState contains the information that the client needs in order to
  // perform a 0-RTT handshake with a server. This information can be reused
  // over several connections to the same server.
  class CachedState {
   public:
    CachedState();
    ~CachedState();

    // is_complete returns true if this object contains enough information to
    // perform a handshake with the server.
    bool is_complete() const;

    // GetServerConfig returns the parsed contents of |server_config|, or NULL
    // if |server_config| is empty. The return value is owned by this object
    // and is destroyed when this object is.
    const CryptoHandshakeMessage* GetServerConfig() const;

    // SetServerConfig checks that |scfg| parses correctly and stores it in
    // |server_config|. It returns true if the parsing succeeds and false
    // otherwise.
    bool SetServerConfig(base::StringPiece scfg);

    const std::string& server_config() const;
    const std::string& source_address_token() const;
    const std::string& orbit() const;

    void set_source_address_token(base::StringPiece token);

   private:
    std::string server_config_id_;  // An opaque id from the server.
    std::string server_config_;  // A serialized handshake message.
    std::string source_address_token_;  // An opaque proof of IP ownership.
    std::string orbit_;  // An opaque server-id used in nonce generation.

    // scfg contains the cached, parsed value of |server_config|.
    mutable scoped_ptr<CryptoHandshakeMessage> scfg_;
  };

  QuicCryptoClientConfig();
  ~QuicCryptoClientConfig();

  // Sets the members to reasonable, default values.
  void SetDefaults();

  // Lookup returns a CachedState for the given hostname, or NULL if no
  // information is known.
  const CachedState* Lookup(const std::string& server_hostname);

  // FillInchoateClientHello sets |out| to be a CHLO message that elicits a
  // source-address token or SCFG from a server. If |cached| is non-NULL, the
  // source-address token will be taken from it.
  void FillInchoateClientHello(const std::string& server_hostname,
                               const CachedState* cached,
                               CryptoHandshakeMessage* out);

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object. This object must have cached enough information about
  // |server_hostname| in order to perform a handshake. This can be checked
  // with the |is_complete| member of |CachedState|.
  //
  // |clock| and |rand| are used to generate the nonce and |out_params| is
  // filled with the results of the handshake that the server is expected to
  // accept.
  QuicErrorCode FillClientHello(const std::string& server_hostname,
                                QuicGuid guid,
                                const CachedState* cached,
                                const QuicClock* clock,
                                QuicRandom* rand,
                                QuicCryptoNegotiatedParameters* out_params,
                                CryptoHandshakeMessage* out,
                                std::string* error_details);

  // ProcessRejection processes a REJ message from a server and updates the
  // cached information about that server. After this, |is_complete| may return
  // true for that server's CachedState.
  QuicErrorCode ProcessRejection(const std::string& server_hostname,
                                 const CryptoHandshakeMessage& rej,
                                 std::string* error_details);

  // ProcessServerHello processes the message in |server_hello|, writes the
  // negotiated parameters to |out_params| and returns QUIC_NO_ERROR. If
  // |server_hello| is unacceptable then it puts an error message in
  // |error_details| and returns an error code.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   const std::string& nonce,
                                   QuicCryptoNegotiatedParameters* out_params,
                                   std::string* error_details);

 private:
  // cached_states_ maps from the server hostname to the cached information
  // about that server.
  std::map<std::string, CachedState*> cached_states_;
};

// QuicCryptoServerConfig contains the crypto configuration of a QUIC server.
// Unlike a client, a QUIC server can have multiple configurations active in
// order to support clients resuming with a previous configuration.
// TODO(agl): when adding configurations at runtime is added, this object will
// need to consider locking.
class NET_EXPORT_PRIVATE QuicCryptoServerConfig {
 public:
  // |source_address_token_secret|: secret key material used for encrypting and
  //     decrypting source address tokens. It can be of any length as it is fed
  //     into a KDF before use.
  explicit QuicCryptoServerConfig(
      base::StringPiece source_address_token_secret);
  ~QuicCryptoServerConfig();

  // ConfigForTesting generates a QuicServerConfigProtobuf protobuf suitable
  // for using in tests. |extra_tags| contains additional key/value pairs that
  // will be inserted into the config.
  static QuicServerConfigProtobuf* ConfigForTesting(
      QuicRandom* rand,
      const QuicClock* clock,
      const CryptoHandshakeMessage& extra_tags);

  // AddConfig adds a QuicServerConfigProtobuf to the availible configurations.
  // It returns the SCFG message from the config if successful. The caller
  // takes ownership of the CryptoHandshakeMessage.
  CryptoHandshakeMessage* AddConfig(QuicServerConfigProtobuf* protobuf);

  // AddTestingConfig creates a test config and then calls AddConfig to add it.
  // Any tags in |extra_tags| will be copied into the config.
  CryptoHandshakeMessage* AddTestingConfig(
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
  // out: the resulting handshake message (either REJ or SHLO)
  // out_params: the state of the handshake
  // error_details: used to store a string describing any error.
  QuicErrorCode ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                                   QuicGuid guid,
                                   const IPEndPoint& client_ip,
                                   QuicTime::Delta now_since_epoch,
                                   QuicRandom* rand,
                                   CryptoHandshakeMessage* out,
                                   QuicCryptoNegotiatedParameters* out_params,
                                   std::string* error_details);

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
                                    QuicTime::Delta now_since_epoch);

  // ValidateSourceAddressToken returns true if the source address token in
  // |token| is a valid and timely token for the IP address |ip| given that the
  // current time is |now|.
  bool ValidateSourceAddressToken(base::StringPiece token,
                                  const IPEndPoint& ip,
                                  QuicTime::Delta now_since_epoch);

  std::map<ServerConfigID, Config*> configs_;

  ServerConfigID active_config_;

  // These members are used to encrypt and decrypt the source address tokens
  // that we receive from and send to clients.
  scoped_ptr<QuicEncrypter> source_address_token_encrypter_;
  scoped_ptr<QuicDecrypter> source_address_token_decrypter_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
