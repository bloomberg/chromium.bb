// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;

namespace test {
class CryptoTestUtils;
}  // namespace test

class NET_EXPORT_PRIVATE QuicCryptoServerStream : public QuicCryptoStream {
 public:
  explicit QuicCryptoServerStream(QuicSession* session);
  virtual ~QuicCryptoServerStream();

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  const QuicNegotiatedParameters& negotiated_params() const;
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params() const;

 private:
  friend class test::CryptoTestUtils;

  // config_ contains non-crypto parameters that are negotiated in the crypto
  // handshake.
  QuicConfig config_;
  // crypto_config_ contains crypto parameters for the handshake.
  QuicCryptoServerConfig crypto_config_;
  std::string server_nonce_;

  QuicNegotiatedParameters negotiated_params_;
  QuicCryptoNegotiatedParameters crypto_negotiated_params_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_SERVER_STREAM_H_
