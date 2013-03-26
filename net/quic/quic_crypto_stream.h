// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_STREAM_H_

#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

class CryptoHandshakeMessage;
class QuicSession;

// Crypto handshake messages in QUIC take place over a reserved
// reliable stream with the id 1.  Each endpoint (client and server)
// will allocate an instance of a subclass of QuicCryptoStream
// to send and receive handshake messages.  (In the normal 1-RTT
// handshake, the client will send a client hello, CHLO, message.
// The server will receive this message and respond with a server
// hello message, SHLO.  At this point both sides will have established
// a crypto context they can use to send encrypted messages.
//
// For more details: http://goto.google.com/quic-crypto
class NET_EXPORT_PRIVATE QuicCryptoStream
    : public ReliableQuicStream,
      public CryptoFramerVisitorInterface {
 public:
  explicit QuicCryptoStream(QuicSession* session);

  // CryptoFramerVisitorInterface implementation
  virtual void OnError(CryptoFramer* framer) OVERRIDE;
  virtual void OnHandshakeMessage(const CryptoHandshakeMessage& message) = 0;

  // ReliableQuicStream implementation
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;

  // Sends |message| to the peer.
  // TODO(wtc): return a success/failure status.
  void SendHandshakeMessage(const CryptoHandshakeMessage& message);

  bool handshake_complete() { return handshake_complete_; }

 protected:
  // Closes the connection
  void CloseConnection(QuicErrorCode error);
  void CloseConnectionWithDetails(QuicErrorCode error, const string& details);

  void SetHandshakeComplete(QuicErrorCode error);

 private:
  CryptoFramer crypto_framer_;
  bool handshake_complete_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoStream);
};

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
      string* error_details) const;

 private:
  // Congestion control feedback type.
  CryptoTagVector congestion_control_;
  // Idle connection state lifetime
  QuicTime::Delta idle_connection_state_lifetime_;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicTime::Delta keepalive_timeout_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_STREAM_H_
