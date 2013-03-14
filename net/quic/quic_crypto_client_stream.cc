// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoClientStream::QuicCryptoClientStream(QuicSession* session,
                                               const string& server_hostname)
    : QuicCryptoStream(session),
      server_hostname_(server_hostname) {
  config_.SetDefaults();

  QuicGuid guid = session->connection()->guid();
  crypto_config_.hkdf_info.append(reinterpret_cast<char*>(&guid),
                                  sizeof(guid));
}

QuicCryptoClientStream::~QuicCryptoClientStream() {
}

void QuicCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  // Do not process handshake messages after the handshake is complete.
  if (handshake_complete()) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return;
  }

  if (message.tag != kSHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  string error_details;
  QuicErrorCode error = config_.ProcessPeerHandshake(
      message,
      CryptoUtils::PEER_PRIORITY,
      &negotiated_params_,
      &error_details);
  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, error_details);
    return;
  }

  QuicErrorCode err = crypto_config_.ProcessServerHello(
      message, nonce_, &crypto_negotiated_params_, &error_details);
  if (err != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(err, error_details);
    return;
  }

  SetHandshakeComplete(QUIC_NO_ERROR);
  return;
}

bool QuicCryptoClientStream::CryptoConnect() {
  crypto_config_.SetDefaults(session()->connection()->random_generator());
  CryptoUtils::GenerateNonce(session()->connection()->clock(),
                             session()->connection()->random_generator(),
                             &nonce_);
  CryptoHandshakeMessage message;
  crypto_config_.FillClientHello(nonce_, server_hostname_, &message);
  config_.ToHandshakeMessage(&message);
  const QuicData& data = message.GetSerialized();
  crypto_config_.hkdf_info.append(data.data(), data.length());
  SendHandshakeMessage(message);
  return true;
}

}  // namespace net
