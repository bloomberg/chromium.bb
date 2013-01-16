// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;
struct CryptoHandshakeMessage;

class NET_EXPORT_PRIVATE QuicCryptoClientStream : public QuicCryptoStream {

 public:
  explicit QuicCryptoClientStream(QuicSession* session);

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  // Performs a crypto handshake with the server. Returns true if the crypto
  // handshake is started successfully.
  bool CryptoConnect();

 private:
  QuicClientCryptoConfig client_crypto_config_;
  // Client's connection nonce (4-byte timestamp + 28 random bytes)
  std::string nonce_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
