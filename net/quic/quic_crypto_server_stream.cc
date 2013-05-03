// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoServerStream::QuicCryptoServerStream(
    const QuicConfig& config,
    const QuicCryptoServerConfig& crypto_config,
    QuicSession* session)
    : QuicCryptoStream(session),
      config_(config),
      crypto_config_(crypto_config) {
}

QuicCryptoServerStream::~QuicCryptoServerStream() {
}

void QuicCryptoServerStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed_) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return;
  }

  if (message.tag() != kCHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  string error_details;
  CryptoHandshakeMessage reply;
  crypto_config_.ProcessClientHello(
      message, session()->connection()->guid(),
      session()->connection()->peer_address(),
      session()->connection()->clock()->NowAsDeltaSinceUnixEpoch(),
      session()->connection()->random_generator(),
      &crypto_negotiated_params_, &reply, &error_details);

  if (reply.tag() == kSHLO) {
    // If we are returning a SHLO then we accepted the handshake.
    QuicErrorCode error = config_.ProcessFinalPeerHandshake(
        message, CryptoUtils::LOCAL_PRIORITY, &negotiated_params_,
        &error_details);
    if (error != QUIC_NO_ERROR) {
      CloseConnectionWithDetails(error, error_details);
      return;
    }

    // Receiving a full CHLO implies the client is prepared to decrypt with
    // the new server write key.  We can start to encrypt with the new server
    // write key.
    //
    // NOTE: the SHLO will be encrypted with the new server write key.
    session()->connection()->SetEncrypter(
        ENCRYPTION_INITIAL,
        crypto_negotiated_params_.encrypter.release());
    session()->connection()->SetDefaultEncryptionLevel(
        ENCRYPTION_INITIAL);
    // Set the decrypter immediately so that we no longer accept unencrypted
    // packets.
    session()->connection()->SetDecrypter(
        crypto_negotiated_params_.decrypter.release());
    encryption_established_ = true;
    handshake_confirmed_ = true;
    session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  }

  SendHandshakeMessage(reply);
  return;
}

const QuicNegotiatedParameters&
QuicCryptoServerStream::negotiated_params() const {
  return negotiated_params_;
}

const QuicCryptoNegotiatedParameters&
QuicCryptoServerStream::crypto_negotiated_params() const {
  return crypto_negotiated_params_;
}

}  // namespace net
