// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream.h"

namespace net {

MockCryptoClientStream::MockCryptoClientStream(QuicSession* session,
                                               const string& server_hostname)
    : QuicCryptoClientStream(session, server_hostname) {
}

MockCryptoClientStream::~MockCryptoClientStream() {
}

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
}

bool MockCryptoClientStream::CryptoConnect() {
  SetHandshakeComplete(QUIC_NO_ERROR);
  return true;
}

}  // namespace net
