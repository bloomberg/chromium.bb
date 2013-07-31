// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class CryptoHandshakeMessage;
class QuicCryptoServerConfig;
class QuicSession;

namespace test {
class CryptoTestUtils;
}  // namespace test

class NET_EXPORT_PRIVATE QuicCryptoServerStream : public QuicCryptoStream {
 public:
  QuicCryptoServerStream(const QuicCryptoServerConfig& crypto_config,
                         QuicSession* session);
  explicit QuicCryptoServerStream(QuicSession* session);
  virtual ~QuicCryptoServerStream();

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  // GetBase64SHA256ClientChannelID sets |*output| to the base64 encoded,
  // SHA-256 hash of the client's ChannelID key and returns true, if the client
  // presented a ChannelID. Otherwise it returns false.
  bool GetBase64SHA256ClientChannelID(std::string* output) const;

 protected:
  virtual QuicErrorCode ProcessClientHello(
      const CryptoHandshakeMessage& message,
      CryptoHandshakeMessage* reply,
      std::string* error_details);

  const QuicCryptoServerConfig* crypto_config() { return &crypto_config_; }

 private:
  friend class test::CryptoTestUtils;

  // crypto_config_ contains crypto parameters for the handshake.
  const QuicCryptoServerConfig& crypto_config_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_
