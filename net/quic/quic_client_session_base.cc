// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session_base.h"

#include "net/quic/quic_flags.h"

namespace net {

QuicClientSessionBase::QuicClientSessionBase(QuicConnection* connection,
                                             const QuicConfig& config)
    : QuicSpdySession(connection, config) {}

QuicClientSessionBase::~QuicClientSessionBase() {}

void QuicClientSessionBase::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  QuicSession::OnCryptoHandshakeEvent(event);
  // Set FEC policy for streams immediately after sending CHLO and before any
  // more data is sent.
  if (!FLAGS_enable_quic_fec || event != ENCRYPTION_FIRST_ESTABLISHED ||
      !config()->HasSendConnectionOptions() ||
      !ContainsQuicTag(config()->SendConnectionOptions(), kFHDR)) {
    return;
  }
  // kFHDR config maps to FEC protection always for headers stream.
  // TODO(jri): Add crypto stream in addition to headers for kHDR.
  headers_stream()->set_fec_policy(FEC_PROTECT_ALWAYS);
}

void QuicClientSessionBase::OnPromiseHeaders(QuicStreamId stream_id,
                                             StringPiece headers_data) {
  QuicSpdyStream* stream = GetSpdyDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnPromiseHeaders(headers_data);
}

void QuicClientSessionBase::OnPromiseHeadersComplete(
    QuicStreamId stream_id,
    QuicStreamId promised_stream_id,
    size_t frame_len) {
  if (promised_stream_id != kInvalidStreamId &&
      promised_stream_id <= largest_promised_stream_id_) {
    CloseConnectionWithDetails(QUIC_INVALID_STREAM_ID,
                               "Received push stream id lesser or equal to the"
                               " last accepted before");
    return;
  }
  largest_promised_stream_id_ = promised_stream_id;

  QuicSpdyStream* stream = GetSpdyDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnPromiseHeadersComplete(promised_stream_id, frame_len);
}

}  // namespace net
