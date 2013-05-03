// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_stream.h"

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_session.h"

using std::string;
using base::StringPiece;

namespace net {

QuicCryptoStream::QuicCryptoStream(QuicSession* session)
    : ReliableQuicStream(kCryptoStreamId, session),
      encryption_established_(false),
      handshake_confirmed_(false) {
  crypto_framer_.set_visitor(this);
}

void QuicCryptoStream::OnError(CryptoFramer* framer) {
  session()->ConnectionClose(framer->error(), false);
}

uint32 QuicCryptoStream::ProcessData(const char* data,
                                     uint32 data_len) {
  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed()) {
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

void QuicCryptoStream::SendHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  const QuicData& data = message.GetSerialized();
  // TODO(wtc): check the return value.
  WriteData(string(data.data(), data.length()), false);
}

}  // namespace net
