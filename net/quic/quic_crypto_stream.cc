// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_stream.h"

#include <string>

#include "base/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_session.h"

using std::string;
using base::StringPiece;

namespace net {

QuicCryptoStream::QuicCryptoStream(QuicSession* session)
    : ReliableQuicStream(kCryptoStreamId, session),
      handshake_complete_(false) {
  crypto_framer_.set_visitor(this);
}

void QuicCryptoStream::OnError(CryptoFramer* framer) {
  session()->ConnectionClose(framer->error(), false);
}

uint32 QuicCryptoStream::ProcessData(const char* data,
                                     uint32 data_len) {
  // Do not process handshake messages after the handshake is complete.
  if (handshake_complete()) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return 0;
  }
  if (!crypto_framer_.ProcessInput(StringPiece(data, data_len))) {
    CloseConnection(crypto_framer_.error());
    return 0;
  }
  return data_len;
}

void QuicCryptoStream::CloseConnection(QuicErrorCode error) {
  session()->connection()->SendConnectionClose(error);
}

void QuicCryptoStream::CloseConnectionWithDetails(QuicErrorCode error,
                                                  const string& details) {
  session()->connection()->SendConnectionCloseWithDetails(error, details);
}

void QuicCryptoStream::SetHandshakeComplete(QuicErrorCode error) {
  handshake_complete_ = true;
  session()->OnCryptoHandshakeComplete(error);
}

void QuicCryptoStream::SendHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  const QuicData& data = message.GetSerialized();
  // TODO(wtc): check the return value.
  WriteData(string(data.data(), data.length()), false);
}

QuicNegotiatedParameters::QuicNegotiatedParameters()
    : idle_connection_state_lifetime(QuicTime::Delta::Zero()),
      keepalive_timeout(QuicTime::Delta::Zero()) {
}

QuicConfig::QuicConfig()
    : idle_connection_state_lifetime(QuicTime::Delta::Zero()),
      keepalive_timeout(QuicTime::Delta::Zero()) {
}

QuicConfig::~QuicConfig() {
}

void QuicConfig::SetDefaults() {
  idle_connection_state_lifetime = QuicTime::Delta::FromSeconds(300);
  keepalive_timeout = QuicTime::Delta::Zero();
  congestion_control.push_back(kQBIC);
}

void QuicConfig::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  out->SetValue(
      kICSL, static_cast<uint32>(idle_connection_state_lifetime.ToSeconds()));
  out->SetValue(kKATO, static_cast<uint32>(keepalive_timeout.ToSeconds()));
  out->SetVector(kCGST, congestion_control);
}

QuicErrorCode QuicConfig::ProcessPeerHandshake(
    const CryptoHandshakeMessage& msg,
    CryptoUtils::Priority priority,
    QuicNegotiatedParameters* out_params,
    string* error_details) const {
  const CryptoTag* their_congestion_controls;
  size_t num_their_congestion_controls;
  QuicErrorCode error;

  error = msg.GetTaglist(kCGST, &their_congestion_controls,
                         &num_their_congestion_controls);
  if (error != QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing CGST";
    }
    return error;
  }

  if (!CryptoUtils::FindMutualTag(congestion_control,
                                  their_congestion_controls,
                                  num_their_congestion_controls,
                                  priority,
                                  &out_params->congestion_control,
                                  NULL)) {
    if (error_details) {
      *error_details = "Unsuported CGST";
    }
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP;
  }

  uint32 idle;
  error = msg.GetUint32(kICSL, &idle);
  if (error != QUIC_NO_ERROR) {
    if (error_details) {
      *error_details = "Missing ICSL";
    }
    return error;
  }

  out_params->idle_connection_state_lifetime = QuicTime::Delta::FromSeconds(
      std::min(static_cast<uint32>(idle_connection_state_lifetime.ToSeconds()),
               idle));

  uint32 keepalive;
  error = msg.GetUint32(kKATO, &keepalive);
  switch (error) {
    case QUIC_NO_ERROR:
      out_params->keepalive_timeout = QuicTime::Delta::FromSeconds(
          std::min(static_cast<uint32>(keepalive_timeout.ToSeconds()),
                   keepalive));
      break;
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      // KATO is optional.
      out_params->keepalive_timeout = QuicTime::Delta::Zero();
      break;
    default:
      if (error_details) {
        *error_details = "Bad KATO";
      }
      return error;
  }

  return QUIC_NO_ERROR;
}

}  // namespace net
