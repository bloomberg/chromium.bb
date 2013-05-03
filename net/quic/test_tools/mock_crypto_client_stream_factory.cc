// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"

#include "base/lazy_instance.h"
#include "net/quic/quic_crypto_client_stream.h"

using std::string;

namespace net {

MockCryptoClientStreamFactory::MockCryptoClientStreamFactory()
  : handshake_mode_(MockCryptoClientStream::CONFIRM_HANDSHAKE) {
}

QuicCryptoClientStream*
MockCryptoClientStreamFactory::CreateQuicCryptoClientStream(
    const string& server_hostname,
    const QuicConfig& config,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config) {
  return new MockCryptoClientStream(server_hostname, config, session,
                                    crypto_config, handshake_mode_);
}

}  // namespace net
