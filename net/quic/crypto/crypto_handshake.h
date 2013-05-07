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
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class KeyExchange;
class ProofVerifier;
class QuicClock;
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
  std::string server_nonce;
};

// QuicCryptoConfig contains common configuration between clients and servers.
class NET_EXPORT_PRIVATE QuicCryptoConfig {
 public:
  enum {
    // CONFIG_VERSION is the one (and, for the moment, only) version number that
    // we implement.
    CONFIG_VERSION = 0,
  };

  // kLabel is constant that is used in key derivation to tie the resulting key
  // to this protocol.
  static const char kLabel[];

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

    // SetProof stores a certificate chain and signature.
    void SetProof(const std::vector<base::StringPiece>& certs,
                  base::StringPiece signature);

    // SetProofValid records that the certificate chain and signature have been
    // validated and that it's safe to assume that the server is legitimate.
    // (Note: this does not check the chain or signature.)
    void SetProofValid();

    const std::string& server_config() const;
    const std::string& source_address_token() const;
    const std::vector<std::string>& certs() const;
    const std::string& signature() const;
    bool proof_valid() const;

    void set_source_address_token(base::StringPiece token);

   private:
    std::string server_config_id_;  // An opaque id from the server.
    std::string server_config_;  // A serialized handshake message.
    std::string source_address_token_;  // An opaque proof of IP ownership.
    std::vector<std::string> certs_;  // A list of certificates in leaf-first
                                      // order.
    std::string server_config_sig_;  // A signature of |server_config_|.
    bool server_config_valid_;  // true if |server_config_| is correctly signed
                                // and |certs_| has been validated.

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
  // source-address token will be taken from it.
  void FillInchoateClientHello(const std::string& server_hostname,
                               const CachedState* cached,
                               CryptoHandshakeMessage* out) const;

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
                                std::string* error_details) const;

  // ProcessRejection processes a REJ message from a server and updates the
  // cached information about that server. After this, |is_complete| may return
  // true for that server's CachedState. If the rejection message contains
  // state about a future handshake (i.e. an nonce value from the server), then
  // it will be saved in |out_params|.
  QuicErrorCode ProcessRejection(CachedState* cached,
                                 const CryptoHandshakeMessage& rej,
                                 QuicCryptoNegotiatedParameters* out_params,
                                 std::string* error_details);

  // ProcessServerHello processes the message in |server_hello|, writes the
  // negotiated parameters to |out_params| and returns QUIC_NO_ERROR. If
  // |server_hello| is unacceptable then it puts an error message in
  // |error_details| and returns an error code.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   const std::string& nonce,
                                   QuicCryptoNegotiatedParameters* out_params,
                                   std::string* error_details);

  const ProofVerifier* proof_verifier() const;

  // SetProofVerifier takes ownership of a |ProofVerifier| that clients are
  // free to use in order to verify certificate chains from servers. Setting a
  // |ProofVerifier| does not alter the behaviour of the
  // QuicCryptoClientConfig, it's just a place to store it.
  void SetProofVerifier(ProofVerifier* verifier);

 private:
  // cached_states_ maps from the server hostname to the cached information
  // about that server.
  std::map<std::string, CachedState*> cached_states_;

  scoped_ptr<ProofVerifier> proof_verifier_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientConfig);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_HANDSHAKE_H_
