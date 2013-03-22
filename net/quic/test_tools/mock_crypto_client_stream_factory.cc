// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"

#include "base/lazy_instance.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/test_tools/mock_crypto_client_stream.h"

using std::string;

namespace net {

QuicCryptoClientStream*
MockCryptoClientStreamFactory::CreateQuicCryptoClientStream(
    QuicSession* session,
    const string& server_hostname) {
  return new MockCryptoClientStream(session, server_hostname);
}

}  // namespace net
