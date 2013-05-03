// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoClientStream::QuicCryptoClientStream(
    const string& server_hostname,
    const QuicConfig& config,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config)
    : QuicCryptoStream(session),
      next_state_(STATE_IDLE),
      num_client_hellos_(0),
      config_(config),
      crypto_config_(crypto_config),
      server_hostname_(server_hostname) {
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

int QuicCryptoClientStream::num_sent_client_hellos() const {
  return num_client_hellos_;
}

// kMaxClientHellos is the maximum number of times that we'll send a client
// hello. The value 3 accounts for:
//   * One failure due to an incorrect or missing source-address token.
//   * One failure due the server's certificate chain being unavailible and the
//     server being unwilling to send it without a valid source-address token.
static const int kMaxClientHellos = 3;

void QuicCryptoClientStream::DoHandshakeLoop(
    const CryptoHandshakeMessage* in) {
  CryptoHandshakeMessage out;
  QuicErrorCode error;
  string error_details;
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_->LookupOrCreate(server_hostname_);

  if (in != NULL) {
    DLOG(INFO) << "Client received: " << in->DebugString();
  }

  for (;;) {
    const State state = next_state_;
    next_state_ = STATE_IDLE;
    switch (state) {
      case STATE_SEND_CHLO: {
        if (num_client_hellos_ > kMaxClientHellos) {
          CloseConnection(QUIC_CRYPTO_TOO_MANY_REJECTS);
          return;
        }
        num_client_hellos_++;

        if (!cached->is_complete()) {
          crypto_config_->FillInchoateClientHello(server_hostname_, cached,
                                                  &out);
          next_state_ = STATE_RECV_REJ;
          DLOG(INFO) << "Client Sending: " << out.DebugString();
          SendHandshakeMessage(out);
          return;
        }
        const CryptoHandshakeMessage* scfg = cached->GetServerConfig();
        config_.ToHandshakeMessage(&out);
        error = crypto_config_->FillClientHello(
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
        // Be prepared to decrypt with the new server write key.
        session()->connection()->SetAlternativeDecrypter(
            crypto_negotiated_params_.decrypter.release(),
            true /* latch once used */);
        // Send subsequent packets under encryption on the assumption that the
        // server will accept the handshake.
        session()->connection()->SetEncrypter(
            ENCRYPTION_INITIAL,
            crypto_negotiated_params_.encrypter.release());
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_INITIAL);
        if (!encryption_established_) {
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
          encryption_established_ = true;
        } else {
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_REESTABLISHED);
        }
        return;
      }
      case STATE_RECV_REJ:
        // We sent a dummy CHLO because we didn't have enough information to
        // perform a handshake, or we sent a full hello that the server
        // rejected. Here we hope to have a REJ that contains the information
        // that we need.
        if (in->tag() != kREJ) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected REJ");
          return;
        }
        error = crypto_config_->ProcessRejection(cached, *in,
                                                 &crypto_negotiated_params_,
                                                 &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        if (!cached->proof_valid()) {
          const ProofVerifier* verifier = crypto_config_->proof_verifier();
          if (!verifier) {
            // If no verifier is set then we don't check the certificates.
            cached->SetProofValid();
          } else if (!cached->signature().empty()) {
            // TODO(rtenneti): In Chromium, we will need to make VerifyProof()
            // asynchronous.
            if (!verifier->VerifyProof(server_hostname_,
                                       cached->server_config(),
                                       cached->certs(),
                                       cached->signature(),
                                       &error_details)) {
              CloseConnectionWithDetails(QUIC_PROOF_INVALID,
                                         "Proof invalid: " + error_details);
              return;
            }
            cached->SetProofValid();
          }
        }
        // Send the subsequent client hello in plaintext.
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_NONE);
        next_state_ = STATE_SEND_CHLO;
        break;
      case STATE_RECV_SHLO:
        // We sent a CHLO that we expected to be accepted and now we're hoping
        // for a SHLO from the server to confirm that.
        if (in->tag() == kREJ) {
          next_state_ = STATE_RECV_REJ;
          break;
        }
        if (in->tag() != kSHLO) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected SHLO or REJ");
          return;
        }
        // TODO(agl): enable this once the tests are corrected to permit it.
        // DCHECK(session()->connection()->alternative_decrypter() == NULL);
        handshake_confirmed_ = true;
        session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
        return;
      case STATE_IDLE:
        // This means that the peer sent us a message that we weren't expecting.
        CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
        return;
    }
  }
}

}  // namespace net
