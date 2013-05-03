// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream.h"

namespace net {

MockCryptoClientStream::MockCryptoClientStream(
    const string& server_hostname,
    const QuicConfig& config,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config,
    HandshakeMode handshake_mode)
  : QuicCryptoClientStream(server_hostname, config, session, crypto_config),
    handshake_mode_(handshake_mode) {
}

MockCryptoClientStream::~MockCryptoClientStream() {
}

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
}

bool MockCryptoClientStream::CryptoConnect() {
  if (handshake_mode_ == ZERO_RTT) {
    encryption_established_ = true;
    handshake_confirmed_ = false;
    session()->OnCryptoHandshakeEvent(
        QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
    return true;
  }

  encryption_established_ = true;
  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  return true;
}

}  // namespace net
