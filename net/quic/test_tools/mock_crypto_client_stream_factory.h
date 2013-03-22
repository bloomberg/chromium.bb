// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <string>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_session.h"

namespace net {

class MockCryptoClientStreamFactory : public QuicCryptoClientStreamFactory  {
 public:
  virtual ~MockCryptoClientStreamFactory() {}

  virtual QuicCryptoClientStream* CreateQuicCryptoClientStream(
      QuicSession* session, const std::string& server_hostname) OVERRIDE;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
