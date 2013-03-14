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

  QuicGuid guid = session->connection()->guid();
  crypto_config_.hkdf_info.append(reinterpret_cast<char*>(&guid),
                                  sizeof(guid));
  // TODO(agl): AddTestingConfig generates a new, random config. In the future
  // this will be replaced with a real source of configs.
  scoped_ptr<CryptoTagValueMap> config_tags(
      crypto_config_.AddTestingConfig(session->connection()->random_generator(),
                                      session->connection()->clock(),
                                      extra_tags));
  // If we were using the same config in many servers then we would have to
  // parse a QuicConfig from config_tags here.
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

  if (message.tag != kCHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  string error_details;
  QuicErrorCode error = config_.ProcessPeerHandshake(
      message, CryptoUtils::LOCAL_PRIORITY, &negotiated_params_,
      &error_details);
  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, "negotiated params");
    return;
  }

  CryptoHandshakeMessage shlo;
  CryptoUtils::GenerateNonce(session()->connection()->clock(),
                             session()->connection()->random_generator(),
                             &server_nonce_);
  crypto_config_.ProcessClientHello(message, server_nonce_, &shlo,
                                    &crypto_negotiated_params_, &error_details);
  if (!error_details.empty()) {
    DLOG(INFO) << "Rejecting CHLO: " << error_details;
  }
  config_.ToHandshakeMessage(&shlo);
  SendHandshakeMessage(shlo);

  // TODO(rch): correctly validate the message
  SetHandshakeComplete(QUIC_NO_ERROR);
  return;
}

}  // namespace net
