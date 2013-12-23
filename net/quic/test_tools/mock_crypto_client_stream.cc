// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockCryptoClientStream::MockCryptoClientStream(
    const string& server_hostname,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config,
    HandshakeMode handshake_mode,
    const SSLInfo* ssl_info)
    : QuicCryptoClientStream(server_hostname, session, crypto_config),
      handshake_mode_(handshake_mode),
      ssl_info_(ssl_info) {
}

MockCryptoClientStream::~MockCryptoClientStream() {
}

void MockCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
}

bool MockCryptoClientStream::CryptoConnect() {
  switch (handshake_mode_) {
    case ZERO_RTT: {
      encryption_established_ = true;
      handshake_confirmed_ = false;
      session()->OnCryptoHandshakeEvent(
          QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
      break;
    }

    case CONFIRM_HANDSHAKE: {
      encryption_established_ = true;
      handshake_confirmed_ = true;
      SetConfigNegotiated();
      session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
      break;
    }

    case COLD_START: {
      handshake_confirmed_ = false;
      encryption_established_ = false;
      break;
    }
  }
  return true;
}

bool MockCryptoClientStream::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!ssl_info_) {
    return false;
  }
  *ssl_info = *ssl_info_;
  return true;
}

void MockCryptoClientStream::SendOnCryptoHandshakeEvent(
    QuicSession::CryptoHandshakeEvent event) {
  encryption_established_ = true;
  if (event == QuicSession::HANDSHAKE_CONFIRMED) {
    handshake_confirmed_ = true;
    SetConfigNegotiated();
  }
  session()->OnCryptoHandshakeEvent(event);
}

void MockCryptoClientStream::SetConfigNegotiated() {
  ASSERT_FALSE(session()->config()->negotiated());
  QuicTagVector cgst;
  cgst.push_back(kINAR);
  cgst.push_back(kQBIC);
  session()->config()->set_congestion_control(cgst, kQBIC);
  session()->config()->set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(2 * kDefaultTimeoutSecs),
      QuicTime::Delta::FromSeconds(kDefaultTimeoutSecs));
  session()->config()->set_max_streams_per_connection(
      2 * kDefaultMaxStreamsPerConnection, kDefaultMaxStreamsPerConnection);

  CryptoHandshakeMessage msg;
  session()->config()->ToHandshakeMessage(&msg);
  string error_details;
  const QuicErrorCode error =
      session()->config()->ProcessClientHello(msg, &error_details);
  ASSERT_EQ(QUIC_NO_ERROR, error);
  ASSERT_TRUE(session()->config()->negotiated());
}

}  // namespace net
