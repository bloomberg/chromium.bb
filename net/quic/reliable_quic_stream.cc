// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "base/logging.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_write_blocked_list.h"

using base::StringPiece;
using std::min;

namespace net {

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

namespace {

struct iovec MakeIovec(StringPiece data) {
  struct iovec iov = {const_cast<char*>(data.data()),
                      static_cast<size_t>(data.size())};
  return iov;
}

}  // namespace

ReliableQuicStream::ReliableQuicStream(QuicStreamId id,
                                       QuicSession* session)
    : sequencer_(this),
      id_(id),
      session_(session),
      stream_bytes_read_(0),
      stream_bytes_written_(0),
      stream_error_(QUIC_STREAM_NO_ERROR),
      connection_error_(QUIC_NO_ERROR),
      read_side_closed_(false),
      write_side_closed_(false),
      fin_buffered_(false),
      fin_sent_(false),
      rst_sent_(false),
      is_server_(session_->is_server()) {
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
    DVLOG(1) << ENDPOINT << "Ignoring frame " << frame.stream_id;
    // We don't want to be reading: blackhole the data.
    return true;
  }
  // Note: This count include duplicate data received.
  stream_bytes_read_ += frame.data.TotalBufferSize();

  bool accepted = sequencer_.OnStreamFrame(frame);

  return accepted;
}

void ReliableQuicStream::OnStreamReset(const QuicRstStreamFrame& frame) {
  stream_error_ = frame.error_code;
  CloseWriteSide();
  CloseReadSide();
}

void ReliableQuicStream::OnConnectionClosed(QuicErrorCode error,
                                            bool from_peer) {
  if (read_side_closed_ && write_side_closed_) {
    return;
  }
  if (error != QUIC_NO_ERROR) {
    stream_error_ = QUIC_STREAM_CONNECTION_ERROR;
    connection_error_ = error;
  }

  CloseWriteSide();
  CloseReadSide();
}

void ReliableQuicStream::OnFinRead() {
  DCHECK(sequencer_.IsClosed());
  CloseReadSide();
}

void ReliableQuicStream::Reset(QuicRstStreamErrorCode error) {
  DCHECK_NE(QUIC_STREAM_NO_ERROR, error);
  stream_error_ = error;
  // Sending a RstStream results in calling CloseStream.
  session()->SendRstStream(id(), error, stream_bytes_written_);
  rst_sent_ = true;
}

void ReliableQuicStream::CloseConnection(QuicErrorCode error) {
  session()->connection()->SendConnectionClose(error);
}

void ReliableQuicStream::CloseConnectionWithDetails(QuicErrorCode error,
                                                    const string& details) {
  session()->connection()->SendConnectionCloseWithDetails(error, details);
}

QuicVersion ReliableQuicStream::version() {
  return session()->connection()->version();
}

void ReliableQuicStream::WriteOrBufferData(StringPiece data, bool fin) {
  if (data.empty() && !fin) {
    LOG(DFATAL) << "data.empty() && !fin";
    return;
  }

  if (fin_buffered_) {
    LOG(DFATAL) << "Fin already buffered";
    return;
  }

  QuicConsumedData consumed_data(0, false);
  fin_buffered_ = fin;

  if (queued_data_.empty()) {
    struct iovec iov(MakeIovec(data));
    consumed_data = WritevData(&iov, 1, fin, NULL);
    DCHECK_LE(consumed_data.bytes_consumed, data.length());
  }

  // If there's unconsumed data or an unconsumed fin, queue it.
  if (consumed_data.bytes_consumed < data.length() ||
      (fin && !consumed_data.fin_consumed)) {
    queued_data_.push_back(
        string(data.data() + consumed_data.bytes_consumed,
               data.length() - consumed_data.bytes_consumed));
  }
}

void ReliableQuicStream::OnCanWrite() {
  bool fin = false;
  while (!queued_data_.empty()) {
    const string& data = queued_data_.front();
    if (queued_data_.size() == 1 && fin_buffered_) {
      fin = true;
    }
    struct iovec iov(MakeIovec(data));
    QuicConsumedData consumed_data = WritevData(&iov, 1, fin, NULL);
    if (consumed_data.bytes_consumed == data.size() &&
        fin == consumed_data.fin_consumed) {
      queued_data_.pop_front();
    } else {
      queued_data_.front().erase(0, consumed_data.bytes_consumed);
      break;
    }
  }
}

QuicConsumedData ReliableQuicStream::WritevData(
    const struct iovec* iov,
    int iov_count,
    bool fin,
    QuicAckNotifier::DelegateInterface* ack_notifier_delegate) {
  if (write_side_closed_) {
    DLOG(ERROR) << ENDPOINT << "Attempt to write when the write side is closed";
    return QuicConsumedData(0, false);
  }

  size_t write_length = 0u;
  for (int i = 0; i < iov_count; ++i) {
    write_length += iov[i].iov_len;
  }
  QuicConsumedData consumed_data = session()->WritevData(
      id(), iov, iov_count, stream_bytes_written_, fin, ack_notifier_delegate);
  stream_bytes_written_ += consumed_data.bytes_consumed;
  if (consumed_data.bytes_consumed == write_length) {
    if (fin && consumed_data.fin_consumed) {
      fin_sent_ = true;
      CloseWriteSide();
    } else if (fin && !consumed_data.fin_consumed) {
      session_->MarkWriteBlocked(id(), EffectivePriority());
    }
  } else {
    session_->MarkWriteBlocked(id(), EffectivePriority());
  }
  return consumed_data;
}

void ReliableQuicStream::CloseReadSide() {
  if (read_side_closed_) {
    return;
  }
  DVLOG(1) << ENDPOINT << "Done reading from stream " << id();

  read_side_closed_ = true;
  if (write_side_closed_) {
    DVLOG(1) << ENDPOINT << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

void ReliableQuicStream::CloseWriteSide() {
  if (write_side_closed_) {
    return;
  }
  DVLOG(1) << ENDPOINT << "Done writing to stream " << id();

  write_side_closed_ = true;
  if (read_side_closed_) {
    DVLOG(1) << ENDPOINT << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

bool ReliableQuicStream::HasBufferedData() {
  return !queued_data_.empty();
}

void ReliableQuicStream::OnClose() {
  CloseReadSide();
  CloseWriteSide();

  if (version() > QUIC_VERSION_13 &&
      !fin_sent_ && !rst_sent_) {
    // For flow control accounting, we must tell the peer how many bytes we have
    // written on this stream before termination. Done here if needed, using a
    // RST frame.
    DVLOG(1) << ENDPOINT << "Sending RST in OnClose: " << id();
    session_->SendRstStream(id(), QUIC_STREAM_NO_ERROR, stream_bytes_written_);
    rst_sent_ = true;
  }
}

}  // namespace net
