// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
#define NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_protocol.h"

namespace net {

class ChannelIDSigner;
class CommonCertSets;
class KeyExchange;
class ProofVerifier;
class QuicDecrypter;
class QuicEncrypter;
class QuicRandom;

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

  // MarkDirty invalidates the cache created by |GetSerialized|.
  void MarkDirty();

  // SetValue sets an element with the given tag to the raw, memory contents of
  // |v|.
  template<class T> void SetValue(QuicTag tag, const T& v) {
    tag_value_map_[tag] =
        std::string(reinterpret_cast<const char*>(&v), sizeof(v));
  }

  // SetVector sets an element with the given tag to the raw contents of an
  // array of elements in |v|.
  template<class T> void SetVector(QuicTag tag, const std::vector<T>& v) {
    if (v.empty()) {
      tag_value_map_[tag] = std::string();
    } else {
      tag_value_map_[tag] = std::string(reinterpret_cast<const char*>(&v[0]),
                                        v.size() * sizeof(T));
    }
  }

  // Returns the message tag.
  QuicTag tag() const { return tag_; }
  // Sets the message tag.
  void set_tag(QuicTag tag) { tag_ = tag; }

  const QuicTagValueMap& tag_value_map() const { return tag_value_map_; }

  void Insert(QuicTagValueMap::const_iterator begin,
              QuicTagValueMap::const_iterator end);

  // SetTaglist sets an element with the given tag to contain a list of tags,
  // passed as varargs. The argument list must be terminated with a 0 element.
  void SetTaglist(QuicTag tag, ...);

  void SetStringPiece(QuicTag tag, base::StringPiece value);

  // Erase removes a tag/value, if present, from the message.
  void Erase(QuicTag tag);

  // GetTaglist finds an element with the given tag containing zero or more
  // tags. If such a tag doesn't exist, it returns false. Otherwise it sets
  // |out_tags| and |out_len| to point to the array of tags and returns true.
  // The array points into the CryptoHandshakeMessage and is valid only for as
  // long as the CryptoHandshakeMessage exists and is not modified.
  QuicErrorCode GetTaglist(QuicTag tag, const QuicTag** out_tags,
                           size_t* out_len) const;

  bool GetStringPiece(QuicTag tag, base::StringPiece* out) const;

  // GetNthValue24 interprets the value with the given tag to be a series of
  // 24-bit, length prefixed values and it returns the subvalue with the given
  // index.
  QuicErrorCode GetNthValue24(QuicTag tag,
                              unsigned index,
                              base::StringPiece* out) const;
  QuicErrorCode GetUint16(QuicTag tag, uint16* out) const;
  QuicErrorCode GetUint32(QuicTag tag, uint32* out) const;
  QuicErrorCode GetUint64(QuicTag tag, uint64* out) const;

  // size returns 4 (message tag) + 2 (uint16, number of entries) +
  // (4 (tag) + 4 (end offset))*tag_value_map_.size() + ∑ value sizes.
  size_t size() const;

  // set_minimum_size sets the minimum number of bytes that the message should
  // consume. The CryptoFramer will add a PAD tag as needed when serializing in
  // order to ensure this. Setting a value of 0 disables padding.
  //
  // Padding is useful in order to ensure that messages are a minimum size. A
  // QUIC server can require a minimum size in order to reduce the
  // amplification factor of any mirror DoS attack.
  void set_minimum_size(size_t min_bytes);

  size_t minimum_size() const;

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
  QuicErrorCode GetPOD(QuicTag tag, void* out, size_t len) const;

  std::string DebugStringInternal(size_t indent) const;

  QuicTag tag_;
  QuicTagValueMap tag_value_map_;

  size_t minimum_size_;

  // The serialized form of the handshake message. This member is constructed
  // lasily.
  mutable scoped_ptr<QuicData> serialized_;
};

// A CrypterPair contains the encrypter and decrypter for an encryption level.
struct NET_EXPORT_PRIVATE CrypterPair {
  CrypterPair();
  ~CrypterPair();
  scoped_ptr<QuicEncrypter> encrypter;
  scoped_ptr<QuicDecrypter> decrypter;
};

// Parameters negotiated by the crypto handshake.
struct NET_EXPORT_PRIVATE QuicCryptoNegotiatedParameters {
  // Initializes the members to 0 or empty values.
  QuicCryptoNegotiatedParameters();
  ~QuicCryptoNegotiatedParameters();

  uint16 version;
  QuicTag key_exchange;
  QuicTag aead;
  std::string initial_premaster_secret;
  std::string forward_secure_premaster_secret;
  CrypterPair initial_crypters;
  CrypterPair forward_secure_crypters;
  // Normalized SNI: converted to lower case and trailing '.' removed.
  std::string sni;
  std::string client_nonce;
  std::string server_nonce;
  // hkdf_input_suffix contains the HKDF input following the label: the GUID,
  // client hello and server config. This is only populated in the client
  // because only the client needs to derive the forward secure keys at a later
  // time from the initial keys.
  std::string hkdf_input_suffix;
  // cached_certs contains the cached certificates that a client used when
  // sending a client hello.
  std::vector<std::string> cached_certs;
  // client_key_exchange is used by clients to store the ephemeral KeyExchange
  // for the connection.
  scoped_ptr<KeyExchange> client_key_exchange;
  // channel_id is set by servers to a ChannelID key when the client correctly
  // proves possession of the corresponding private key. It consists of 32
  // bytes of x coordinate, followed by 32 bytes of y coordinate. Both values
  // are big-endian and the pair is a P-256 public key.
  std::string channel_id;
};

// QuicCryptoConfig contains common configuration between clients and servers.
class NET_EXPORT_PRIVATE QuicCryptoConfig {
 public:
  enum {
    // CONFIG_VERSION is the one (and, for the moment, only) version number that
    // we implement.
    CONFIG_VERSION = 0,
  };

  // kInitialLabel is a constant that is used when deriving the initial
  // (non-forward secure) keys for the connection in order to tie the resulting
  // key to this protocol.
  static const char kInitialLabel[];

  // kCETVLabel is a constant that is used when deriving the keys for the
  // encrypted tag/value block in the client hello.
  static const char kCETVLabel[];

  // kForwardSecureLabel is a constant that is used when deriving the forward
  // secure keys for the connection in order to tie the resulting key to this
  // protocol.
  static const char kForwardSecureLabel[];

  QuicCryptoConfig();
  ~QuicCryptoConfig();

  // Protocol version
  uint16 version;
  // Key exchange methods. The following two members' values correspond by
  // index.
  QuicTagVector kexs;
  // Authenticated encryption with associated data (AEAD) algorithms.
  QuicTagVector aead;

  const CommonCertSets* common_cert_sets;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoConfig);
};

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client. Note that this object isn't thread-safe. It's designed to be used on
// a single thread at a time.
class NET_EXPORT_PRIVATE QuicCryptoClientConfig : public QuicCryptoConfig {
 public:
  // A CachedState contains the information that the client needs in order to
  // perform a 0-RTT handshake with a server. This information can be reused
  // over several connections to the same server.
  class CachedState {
   public:
    CachedState();
    ~CachedState();

    // IsComplete returns true if this object contains enough information to
    // perform a handshake with the server. |now| is used to judge whether any
    // cached server config has expired.
    bool IsComplete(QuicWallTime now) const;

    // GetServerConfig returns the parsed contents of |server_config|, or NULL
    // if |server_config| is empty. The return value is owned by this object
    // and is destroyed when this object is.
    const CryptoHandshakeMessage* GetServerConfig() const;

    // SetServerConfig checks that |server_config| parses correctly and stores
    // it in |server_config_|. |now| is used to judge whether |server_config|
    // has expired.
    QuicErrorCode SetServerConfig(base::StringPiece server_config,
                                  QuicWallTime now,
                                  std::string* error_details);

    // InvalidateServerConfig clears the cached server config (if any).
    void InvalidateServerConfig();

    // SetProof stores a certificate chain and signature.
    void SetProof(const std::vector<std::string>& certs,
                  base::StringPiece signature);

    // SetProofValid records that the certificate chain and signature have been
    // validated and that it's safe to assume that the server is legitimate.
    // (Note: this does not check the chain or signature.)
    void SetProofValid();

    // If the server config or the proof has changed then it needs to be
    // revalidated. Helper function to keep server_config_valid_ and
    // generation_counter_ in sync.
    void SetProofInvalid();

    const std::string& server_config() const;
    const std::string& source_address_token() const;
    const std::vector<std::string>& certs() const;
    const std::string& signature() const;
    bool proof_valid() const;
    uint64 generation_counter() const;
    const ProofVerifyDetails* proof_verify_details() const;

    void set_source_address_token(base::StringPiece token);

    // SetProofVerifyDetails takes ownership of |details|.
    void SetProofVerifyDetails(ProofVerifyDetails* details);

   private:
    std::string server_config_id_;      // An opaque id from the server.
    std::string server_config_;         // A serialized handshake message.
    std::string source_address_token_;  // An opaque proof of IP ownership.
    std::vector<std::string> certs_;    // A list of certificates in leaf-first
                                        // order.
    std::string server_config_sig_;     // A signature of |server_config_|.
    bool server_config_valid_;          // True if |server_config_| is correctly
                                        // signed and |certs_| has been
                                        // validated.
    // Generation counter associated with the |server_config_|, |certs_| and
    // |server_config_sig_| combination. It is incremented whenever we set
    // server_config_valid_ to false.
    uint64 generation_counter_;

    scoped_ptr<ProofVerifyDetails> proof_verify_details_;

    // scfg contains the cached, parsed value of |server_config|.
    mutable scoped_ptr<CryptoHandshakeMessage> scfg_;
  };

  QuicCryptoClientConfig();
  ~QuicCryptoClientConfig();

  // Sets the members to reasonable, default values.
  void SetDefaults();

  // LookupOrCreate returns a CachedState for the given hostname. If no such
  // CachedState currently exists, it will be created and cached.
  CachedState* LookupOrCreate(const std::string& server_hostname);

  // FillInchoateClientHello sets |out| to be a CHLO message that elicits a
  // source-address token or SCFG from a server. If |cached| is non-NULL, the
  // source-address token will be taken from it. |out_params| is used in order
  // to store the cached certs that were sent as hints to the server in
  // |out_params->cached_certs|.
  void FillInchoateClientHello(const std::string& server_hostname,
                               const CachedState* cached,
                               QuicCryptoNegotiatedParameters* out_params,
                               CryptoHandshakeMessage* out) const;

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object. This object must have cached enough information about
  // |server_hostname| in order to perform a handshake. This can be checked
  // with the |IsComplete| member of |CachedState|.
  //
  // |clock| and |rand| are used to generate the nonce and |out_params| is
  // filled with the results of the handshake that the server is expected to
  // accept.
  QuicErrorCode FillClientHello(const std::string& server_hostname,
                                QuicGuid guid,
                                const CachedState* cached,
                                QuicWallTime now,
                                QuicRandom* rand,
                                QuicCryptoNegotiatedParameters* out_params,
                                CryptoHandshakeMessage* out,
                                std::string* error_details) const;

  // ProcessRejection processes a REJ message from a server and updates the
  // cached information about that server. After this, |IsComplete| may return
  // true for that server's CachedState. If the rejection message contains
  // state about a future handshake (i.e. an nonce value from the server), then
  // it will be saved in |out_params|. |now| is used to judge whether the
  // server config in the rejection message has expired.
  QuicErrorCode ProcessRejection(CachedState* cached,
                                 const CryptoHandshakeMessage& rej,
                                 QuicWallTime now,
                                 QuicCryptoNegotiatedParameters* out_params,
                                 std::string* error_details);

  // ProcessServerHello processes the message in |server_hello|, writes the
  // negotiated parameters to |out_params| and returns QUIC_NO_ERROR. If
  // |server_hello| is unacceptable then it puts an error message in
  // |error_details| and returns an error code.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   QuicGuid guid,
                                   QuicCryptoNegotiatedParameters* out_params,
                                   std::string* error_details);

  ProofVerifier* proof_verifier() const;

  // SetProofVerifier takes ownership of a |ProofVerifier| that clients are
  // free to use in order to verify certificate chains from servers. If a
  // ProofVerifier is set then the client will request a certificate chain from
  // the server.
  void SetProofVerifier(ProofVerifier* verifier);

  ChannelIDSigner* channel_id_signer() const;

  // SetChannelIDSigner sets a ChannelIDSigner that will be called when the
  // server supports channel IDs to sign a message proving possession of the
  // given ChannelID. This object takes ownership of |signer|.
  void SetChannelIDSigner(ChannelIDSigner* signer);

 private:
  // cached_states_ maps from the server hostname to the cached information
  // about that server.
  std::map<std::string, CachedState*> cached_states_;

  scoped_ptr<ProofVerifier> proof_verifier_;
  scoped_ptr<ChannelIDSigner> channel_id_signer_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientConfig);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
