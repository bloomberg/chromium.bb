// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_session.h"

namespace net {

class MockCryptoClientStream : public QuicCryptoClientStream {
 public:
  MockCryptoClientStream(QuicSession* session,
                         const std::string& server_hostname);
  virtual ~MockCryptoClientStream();

  // CryptoFramerVisitorInterface implementation.
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  // QuicCryptoClientStream implementation.
  virtual bool CryptoConnect() OVERRIDE;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_H_
