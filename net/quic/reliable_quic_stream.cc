// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "net/quic/quic_session.h"

using base::StringPiece;

namespace net {

ReliableQuicStream::ReliableQuicStream(QuicStreamId id,
                                       QuicSession* session)
    : sequencer_(this),
      id_(id),
      session_(session),
      visitor_(NULL),
      stream_bytes_read_(0),
      stream_bytes_written_(0),
      stream_error_(QUIC_STREAM_NO_ERROR),
      connection_error_(QUIC_NO_ERROR),
      read_side_closed_(false),
      write_side_closed_(false),
      fin_buffered_(false),
      fin_sent_(false) {
}

ReliableQuicStream::~ReliableQuicStream() {
}

bool ReliableQuicStream::WillAcceptStreamFrame(
    const QuicStreamFrame& frame) const {
  if (read_side_closed_) {
    return true;
  }
  if (frame.stream_id != id_) {
    LOG(ERROR) << "Error!";
    return false;
  }
  return sequencer_.WillAcceptStreamFrame(frame);
}

bool ReliableQuicStream::OnStreamFrame(const QuicStreamFrame& frame) {
  DCHECK_EQ(frame.stream_id, id_);
  if (read_side_closed_) {
    DLOG(INFO) << "Ignoring frame " << frame.stream_id;
    // We don't want to be reading: blackhole the data.
    return true;
  }
  // Note: This count include duplicate data received.
  stream_bytes_read_ += frame.data.length();

  bool accepted = sequencer_.OnStreamFrame(frame);

  if (frame.fin) {
    sequencer_.CloseStreamAtOffset(frame.offset + frame.data.size(), true);
  }

  return accepted;
}

void ReliableQuicStream::OnStreamReset(QuicRstStreamErrorCode error) {
  stream_error_ = error;
  TerminateFromPeer(false);  // Full close.
}

void ReliableQuicStream::ConnectionClose(QuicErrorCode error, bool from_peer) {
  if (error != QUIC_NO_ERROR) {
    stream_error_ = QUIC_STREAM_CONNECTION_ERROR;
    connection_error_ = error;
  }

  if (from_peer) {
    TerminateFromPeer(false);
  } else {
    CloseWriteSide();
    CloseReadSide();
  }
}

void ReliableQuicStream::TerminateFromPeer(bool half_close) {
  if (!half_close) {
    CloseWriteSide();
  }
  CloseReadSide();
}

void ReliableQuicStream::Close(QuicRstStreamErrorCode error) {
  stream_error_ = error;
  session()->SendRstStream(id(), error);
}

bool ReliableQuicStream::IsHalfClosed() const {
  return sequencer_.IsHalfClosed();
}

bool ReliableQuicStream::HasBytesToRead() const {
  return sequencer_.HasBytesToRead();
}

const IPEndPoint& ReliableQuicStream::GetPeerAddress() const {
  return session_->peer_address();
}

QuicConsumedData ReliableQuicStream::WriteData(StringPiece data, bool fin) {
  return WriteOrBuffer(data, fin);
}

QuicConsumedData ReliableQuicStream::WriteOrBuffer(StringPiece data, bool fin) {
  DCHECK(!fin_buffered_);

  QuicConsumedData consumed_data(0, false);
  fin_buffered_ = fin;

  if (queued_data_.empty()) {
    consumed_data = WriteDataInternal(string(data.data(), data.length()), fin);
    DCHECK_LE(consumed_data.bytes_consumed, data.length());
  }

  // If there's unconsumed data or an unconsumed fin, queue it.
  if (consumed_data.bytes_consumed < data.length() ||
      (fin && !consumed_data.fin_consumed)) {
    queued_data_.push_back(
        string(data.data() + consumed_data.bytes_consumed,
               data.length() - consumed_data.bytes_consumed));
  }

  return QuicConsumedData(data.size(), true);
}

void ReliableQuicStream::OnCanWrite() {
  bool fin = false;
  while (!queued_data_.empty()) {
    const string& data = queued_data_.front();
    if (queued_data_.size() == 1 && fin_buffered_) {
      fin = true;
    }
    QuicConsumedData consumed_data = WriteDataInternal(data, fin);
    if (consumed_data.bytes_consumed == data.size() &&
        fin == consumed_data.fin_consumed) {
      queued_data_.pop_front();
    } else {
      queued_data_.front().erase(0, consumed_data.bytes_consumed);
      break;
    }
  }
}

QuicConsumedData ReliableQuicStream::WriteDataInternal(
    StringPiece data, bool fin) {
  if (write_side_closed_) {
    DLOG(ERROR) << "Attempt to write when the write side is closed";
    return QuicConsumedData(0, false);
  }

  QuicConsumedData consumed_data =
      session()->WriteData(id(), data, stream_bytes_written_, fin);
  stream_bytes_written_ += consumed_data.bytes_consumed;
  if (consumed_data.bytes_consumed == data.length()) {
    if (fin && consumed_data.fin_consumed) {
      fin_sent_ = true;
      CloseWriteSide();
    }
  } else {
    session_->MarkWriteBlocked(id());
  }
  return consumed_data;
}

void ReliableQuicStream::CloseReadSide() {
  if (read_side_closed_) {
    return;
  }
  DLOG(INFO) << "Done reading from stream " << id();

  read_side_closed_ = true;
  if (write_side_closed_) {
    DLOG(INFO) << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

void ReliableQuicStream::CloseWriteSide() {
  if (write_side_closed_) {
    return;
  }
  DLOG(INFO) << "Done writing to stream " << id();

  write_side_closed_ = true;
  if (read_side_closed_) {
    DLOG(INFO) << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

void ReliableQuicStream::OnClose() {
  if (visitor_) {
    Visitor* visitor = visitor_;
    // Calling Visitor::OnClose() may result the destruction of the visitor,
    // so we need to ensure we don't call it again.
    visitor_ = NULL;
    visitor->OnClose(this);
  }
}

}  // namespace net
