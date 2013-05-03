// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <string>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_session.h"
#include "net/quic/test_tools/mock_crypto_client_stream.h"

namespace net {

class MockCryptoClientStreamFactory : public QuicCryptoClientStreamFactory  {
 public:
  MockCryptoClientStreamFactory();

  virtual ~MockCryptoClientStreamFactory() {}

  virtual QuicCryptoClientStream* CreateQuicCryptoClientStream(
      const string& server_hostname,
      const QuicConfig& config,
      QuicSession* session,
      QuicCryptoClientConfig* crypto_config) OVERRIDE;

  void set_handshake_mode(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    handshake_mode_ = handshake_mode;
  }

 private:
  MockCryptoClientStream::HandshakeMode handshake_mode_;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
