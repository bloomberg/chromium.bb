// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoServerStream::QuicCryptoServerStream(QuicSession* session)
    : QuicCryptoStream(session) {
  config_.SetDefaults();
  // Use hardcoded crypto parameters for now.
  CryptoHandshakeMessage extra_tags;
  config_.ToHandshakeMessage(&extra_tags);

  // TODO(agl): AddTestingConfig generates a new, random config. In the future
  // this will be replaced with a real source of configs.
  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config_.AddTestingConfig(session->connection()->random_generator(),
                                      session->connection()->clock(),
                                      extra_tags));
  // If we were using the same config in many servers then we would have to
  // parse a QuicConfig from config_tags here.

  // Our non-crypto configuration is also expressed in the SCFG because it's
  // signed. Thus |config_| needs to be consistent with that.
  if (!config_.SetFromHandshakeMessage(*scfg)) {
    // TODO(agl): when we aren't generating testing configs then this can be a
    // CHECK at startup time.
    LOG(WARNING) << "SCFG could not be parsed by QuicConfig.";
    DCHECK(false);
  }
}

QuicCryptoServerStream::~QuicCryptoServerStream() {
}

void QuicCryptoServerStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  // Do not process handshake messages after the handshake is complete.
  if (handshake_complete()) {
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
      message, session()->connection()->guid(), &reply,
      &crypto_negotiated_params_, &error_details);

  if (reply.tag() == kSHLO) {
    // If we are returning a SHLO then we accepted the handshake.
    QuicErrorCode error = config_.ProcessFinalPeerHandshake(
        message, CryptoUtils::LOCAL_PRIORITY, &negotiated_params_,
        &error_details);
    if (error != QUIC_NO_ERROR) {
      CloseConnectionWithDetails(error, error_details);
      return;
    }

    SetHandshakeComplete(QUIC_NO_ERROR);
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
