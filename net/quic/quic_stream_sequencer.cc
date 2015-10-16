// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_frame_list.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/quic/reliable_quic_stream.h"

using std::min;
using std::numeric_limits;
using std::string;

namespace net {

QuicStreamSequencer::QuicStreamSequencer(ReliableQuicStream* quic_stream,
                                         const QuicClock* clock)
    : stream_(quic_stream),
      num_bytes_consumed_(0),
      close_offset_(numeric_limits<QuicStreamOffset>::max()),
      blocked_(false),
      num_bytes_buffered_(0),
      num_frames_received_(0),
      num_duplicate_frames_received_(0),
      num_early_frames_received_(0),
      clock_(clock) {}

QuicStreamSequencer::~QuicStreamSequencer() {}

void QuicStreamSequencer::OnStreamFrame(const QuicStreamFrame& frame) {
  ++num_frames_received_;
  const QuicStreamOffset byte_offset = frame.offset;
  const size_t data_len = frame.data.length();
  if (data_len == 0 && !frame.fin) {
    // Stream frames must have data or a fin flag.
    stream_->CloseConnectionWithDetails(QUIC_INVALID_STREAM_FRAME,
                                        "Empty stream frame without FIN set.");
    return;
  }

  if (frame.fin) {
    CloseStreamAtOffset(frame.offset + data_len);
    if (data_len == 0) {
      return;
    }
  }
  size_t bytes_written;
  QuicErrorCode result = buffered_frames_.WriteAtOffset(
      byte_offset, frame.data, clock_->ApproximateNow(), &bytes_written);

  if (result == QUIC_INVALID_STREAM_DATA) {
    stream_->CloseConnectionWithDetails(
        QUIC_INVALID_STREAM_FRAME, "Stream frame overlaps with buffered data.");
    return;
  }
  if (result == QUIC_NO_ERROR && bytes_written == 0) {
    ++num_duplicate_frames_received_;
    // Silently ignore duplicates.
    return;
  }

  if (byte_offset > num_bytes_consumed_) {
    ++num_early_frames_received_;
  }

  num_bytes_buffered_ += data_len;

  if (blocked_) {
    return;
  }

  if (byte_offset == num_bytes_consumed_) {
    stream_->OnDataAvailable();
  }
}

void QuicStreamSequencer::CloseStreamAtOffset(QuicStreamOffset offset) {
  const QuicStreamOffset kMaxOffset = numeric_limits<QuicStreamOffset>::max();

  // If there is a scheduled close, the new offset should match it.
  if (close_offset_ != kMaxOffset && offset != close_offset_) {
    stream_->Reset(QUIC_MULTIPLE_TERMINATION_OFFSETS);
    return;
  }

  close_offset_ = offset;

  MaybeCloseStream();
}

bool QuicStreamSequencer::MaybeCloseStream() {
  if (!blocked_ && IsClosed()) {
    DVLOG(1) << "Passing up termination, as we've processed "
             << num_bytes_consumed_ << " of " << close_offset_ << " bytes.";
    // This will cause the stream to consume the fin.
    // Technically it's an error if num_bytes_consumed isn't exactly
    // equal, but error handling seems silly at this point.
    stream_->OnDataAvailable();
    buffered_frames_.Clear();
    num_bytes_buffered_ = 0;
    return true;
  }
  return false;
}

int QuicStreamSequencer::GetReadableRegions(iovec* iov, size_t iov_len) const {
  DCHECK(!blocked_);
  return buffered_frames_.GetReadableRegions(iov, iov_len);
}

bool QuicStreamSequencer::GetReadableRegion(iovec* iov,
                                            QuicTime* timestamp) const {
  DCHECK(!blocked_);
  return buffered_frames_.GetReadableRegion(iov, timestamp);
}

int QuicStreamSequencer::Readv(const struct iovec* iov, size_t iov_len) {
  DCHECK(!blocked_);
  size_t bytes_read = buffered_frames_.ReadvAndInvalidate(iov, iov_len);
  RecordBytesConsumed(bytes_read);
  return static_cast<int>(bytes_read);
}

bool QuicStreamSequencer::HasBytesToRead() const {
  return buffered_frames_.HasBytesToRead();
}

bool QuicStreamSequencer::IsClosed() const {
  return num_bytes_consumed_ >= close_offset_;
}

void QuicStreamSequencer::MarkConsumed(size_t num_bytes_consumed) {
  DCHECK(!blocked_);
  bool result =
      buffered_frames_.IncreaseTotalReadAndInvalidate(num_bytes_consumed);
  if (!result) {
    LOG(DFATAL) << "Invalid argument to MarkConsumed."
                << " expect to consume: " << num_bytes_consumed
                << ", but not enough bytes available.";
    stream_->Reset(QUIC_ERROR_PROCESSING_STREAM);
    return;
  }
  RecordBytesConsumed(num_bytes_consumed);
}

void QuicStreamSequencer::SetBlockedUntilFlush() {
  blocked_ = true;
}

void QuicStreamSequencer::SetUnblocked() {
  blocked_ = false;
  if (IsClosed() || HasBytesToRead()) {
    stream_->OnDataAvailable();
  }
}

void QuicStreamSequencer::RecordBytesConsumed(size_t bytes_consumed) {
  num_bytes_consumed_ += bytes_consumed;
  num_bytes_buffered_ -= bytes_consumed;

  stream_->AddBytesConsumed(bytes_consumed);
}

}  // namespace net
