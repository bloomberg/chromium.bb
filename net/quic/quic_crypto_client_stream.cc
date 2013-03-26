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
      next_state_(STATE_IDLE),
      server_hostname_(server_hostname) {
  config_.SetDefaults();
  crypto_config_.SetDefaults();
}

QuicCryptoClientStream::~QuicCryptoClientStream() {
}

void QuicCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  DoHandshakeLoop(&message);
}

bool QuicCryptoClientStream::CryptoConnect() {
  next_state_ = STATE_SEND_CHLO;
  DoHandshakeLoop(NULL);
  return true;
}

const QuicNegotiatedParameters&
QuicCryptoClientStream::negotiated_params() const {
  return negotiated_params_;
}

const QuicCryptoNegotiatedParameters&
QuicCryptoClientStream::crypto_negotiated_params() const {
  return crypto_negotiated_params_;
}

void QuicCryptoClientStream::DoHandshakeLoop(
    const CryptoHandshakeMessage* in) {
  CryptoHandshakeMessage out;
  QuicErrorCode error;
  string error_details;

  if (in != NULL) {
    DLOG(INFO) << "Client received: " << in->DebugString();
  }

  for (;;) {
    const State state = next_state_;
    next_state_ = STATE_IDLE;
    switch (state) {
      case STATE_SEND_CHLO: {
        const QuicCryptoClientConfig::CachedState* cached =
            crypto_config_.Lookup(server_hostname_);
        if (!cached || !cached->is_complete()) {
          crypto_config_.FillInchoateClientHello(server_hostname_, cached,
                                                 &out);
          next_state_ = STATE_RECV_REJ;
          DLOG(INFO) << "Client Sending: " << out.DebugString();
          SendHandshakeMessage(out);
          return;
        }
        const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
        config_.ToHandshakeMessage(&out);
        error = crypto_config_.FillClientHello(
            server_hostname_,
            session()->connection()->guid(),
            cached,
            session()->connection()->clock(),
            session()->connection()->random_generator(),
            &crypto_negotiated_params_,
            &out,
            &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        error = config_.ProcessFinalPeerHandshake(
            *scfg, CryptoUtils::PEER_PRIORITY, &negotiated_params_,
            &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        next_state_ = STATE_RECV_SHLO;
        DLOG(INFO) << "Client Sending: " << out.DebugString();
        SendHandshakeMessage(out);
        return;
      }
      case STATE_RECV_REJ:
        // We sent a dummy CHLO because we don't have enough information to
        // perform a handshake. Here we hope to have a REJ that contains the
        // information that we need.
        if (in->tag() != kREJ) {
            CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                       "Expected REJ");
            return;
        }
        error = crypto_config_.ProcessRejection(server_hostname_, *in,
                                              &error_details);
        if (error != QUIC_NO_ERROR) {
            CloseConnectionWithDetails(error, error_details);
            return;
        }
        next_state_ = STATE_SEND_CHLO;
        break;
      case STATE_RECV_SHLO:
        // We sent a CHLO that we expected to be accepted and now we're hoping
        // for a SHLO from the server to confirm that.
        if (in->tag() != kSHLO) {
          // TODO(agl): in the future we would attempt the handshake again.
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected SHLO");
          return;
        }
        SetHandshakeComplete(QUIC_NO_ERROR);
        return;
      case STATE_IDLE:
        // This means that the peer sent us a message that we weren't expecting.
        CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
        return;
    }
  }
}

}  // namespace net
