// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "net/quic/reliable_quic_stream.h"

using std::min;
using std::numeric_limits;
using std::string;

namespace net {

QuicStreamSequencer::FrameData::FrameData(QuicStreamOffset offset,
                                          const string& segment)
    : offset(offset), segment(segment) {}

QuicStreamSequencer::QuicStreamSequencer(ReliableQuicStream* quic_stream)
    : stream_(quic_stream),
      num_bytes_consumed_(0),
      close_offset_(numeric_limits<QuicStreamOffset>::max()),
      blocked_(false),
      num_bytes_buffered_(0),
      num_frames_received_(0),
      num_duplicate_frames_received_(0),
      num_early_frames_received_(0) {
}

QuicStreamSequencer::~QuicStreamSequencer() {
}

void QuicStreamSequencer::OnStreamFrame(const QuicStreamFrame& frame) {
  ++num_frames_received_;
  FrameList::iterator insertion_point = FindInsertionPoint(frame);
  if (IsDuplicate(frame, insertion_point)) {
    ++num_duplicate_frames_received_;
    // Silently ignore duplicates.
    return;
  }

  if (FrameOverlapsBufferedData(frame, insertion_point)) {
    stream_->CloseConnectionWithDetails(
        QUIC_INVALID_STREAM_FRAME, "Stream frame overlaps with buffered data.");
    return;
  }

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

  if (byte_offset > num_bytes_consumed_) {
    ++num_early_frames_received_;
  }

  DVLOG(1) << "Buffering stream data at offset " << byte_offset;
  // Inserting an empty string and then copying to avoid the extra copy.
  insertion_point =
      buffered_frames_.insert(insertion_point, FrameData(byte_offset, ""));
  frame.data.CopyToString(&insertion_point->segment);
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
             << num_bytes_consumed_ << " of " << close_offset_
             << " bytes.";
    // This will cause the stream to consume the fin.
    // Technically it's an error if num_bytes_consumed isn't exactly
    // equal, but error handling seems silly at this point.
    stream_->OnDataAvailable();
    buffered_frames_.clear();
    num_bytes_buffered_ = 0;
    return true;
  }
  return false;
}

int QuicStreamSequencer::GetReadableRegions(iovec* iov, size_t iov_len) const {
  DCHECK(!blocked_);
  FrameList::const_iterator it = buffered_frames_.begin();
  size_t index = 0;
  QuicStreamOffset offset = num_bytes_consumed_;
  while (it != buffered_frames_.end() && index < iov_len) {
    if (it->offset != offset) {
      return index;
    }

    iov[index].iov_base =
        static_cast<void*>(const_cast<char*>(it->segment.data()));
    iov[index].iov_len = it->segment.size();
    offset += it->segment.size();

    ++index;
    ++it;
  }
  return index;
}

int QuicStreamSequencer::Readv(const struct iovec* iov, size_t iov_len) {
  DCHECK(!blocked_);
  FrameList::iterator it = buffered_frames_.begin();
  size_t iov_index = 0;
  size_t iov_offset = 0;
  size_t frame_offset = 0;
  QuicStreamOffset initial_bytes_consumed = num_bytes_consumed_;

  while (iov_index < iov_len && it != buffered_frames_.end() &&
         it->offset == num_bytes_consumed_) {
    int bytes_to_read = min(iov[iov_index].iov_len - iov_offset,
                            it->segment.size() - frame_offset);

    char* iov_ptr = static_cast<char*>(iov[iov_index].iov_base) + iov_offset;
    memcpy(iov_ptr, it->segment.data() + frame_offset, bytes_to_read);
    frame_offset += bytes_to_read;
    iov_offset += bytes_to_read;

    if (iov[iov_index].iov_len == iov_offset) {
      // We've filled this buffer.
      iov_offset = 0;
      ++iov_index;
    }
    if (it->segment.size() == frame_offset) {
      // We've copied this whole frame
      RecordBytesConsumed(it->segment.size());
      buffered_frames_.erase(it);
      it = buffered_frames_.begin();
      frame_offset = 0;
    }
  }
  // Done copying.  If there is a partial frame, update it.
  if (frame_offset != 0) {
    buffered_frames_.push_front(
        FrameData(it->offset + frame_offset, it->segment.substr(frame_offset)));
    buffered_frames_.erase(it);
    RecordBytesConsumed(frame_offset);
  }
  return static_cast<int>(num_bytes_consumed_ - initial_bytes_consumed);
}

bool QuicStreamSequencer::HasBytesToRead() const {
  return !buffered_frames_.empty() &&
         buffered_frames_.begin()->offset == num_bytes_consumed_;
}

bool QuicStreamSequencer::IsClosed() const {
  return num_bytes_consumed_ >= close_offset_;
}

QuicStreamSequencer::FrameList::iterator
QuicStreamSequencer::FindInsertionPoint(const QuicStreamFrame& frame) {
  if (buffered_frames_.empty()) {
    return buffered_frames_.begin();
  }
  // If it's after all buffered_frames, return the end.
  if (frame.offset >= (buffered_frames_.rbegin()->offset +
                       buffered_frames_.rbegin()->segment.length())) {
    return buffered_frames_.end();
  }
  FrameList::iterator iter = buffered_frames_.begin();
  // Only advance the iterator if the data begins after the already received
  // frame.  If the new frame overlaps with an existing frame, the iterator will
  // still point to the frame it overlaps with.
  while (iter != buffered_frames_.end() &&
         frame.offset >= iter->offset + iter->segment.length()) {
    ++iter;
  }
  return iter;
}

bool QuicStreamSequencer::FrameOverlapsBufferedData(
    const QuicStreamFrame& frame,
    FrameList::const_iterator insertion_point) const {
  if (buffered_frames_.empty() || insertion_point == buffered_frames_.end()) {
    return false;
  }
  // If there is a buffered frame with a higher starting offset, then check to
  // see if the new frame overlaps the beginning of the higher frame.
  if (frame.offset < insertion_point->offset &&
      frame.offset + frame.data.length() > insertion_point->offset) {
    DVLOG(1) << "New frame overlaps next frame: " << frame.offset << " + "
             << frame.data.size() << " > " << insertion_point->offset;
    return true;
  }
  // If there is a buffered frame with a lower starting offset, then check to
  // see if the buffered frame runs into the new frame.
  if (frame.offset >= insertion_point->offset &&
      frame.offset <
          insertion_point->offset + insertion_point->segment.length()) {
    DVLOG(1) << "Preceeding frame overlaps new frame: "
             << insertion_point->offset << " + "
             << insertion_point->segment.length() << " > " << frame.offset;
    return true;
  }

  return false;
}

void QuicStreamSequencer::MarkConsumed(size_t num_bytes_consumed) {
  DCHECK(!blocked_);
  size_t end_offset = num_bytes_consumed_ + num_bytes_consumed;
  while (!buffered_frames_.empty() && end_offset != num_bytes_consumed_) {
    FrameList::iterator it = buffered_frames_.begin();
    if (it->offset != num_bytes_consumed_) {
      LOG(DFATAL) << "Invalid argument to MarkConsumed. "
                  << " num_bytes_consumed_: " << num_bytes_consumed_
                  << " end_offset: " << end_offset << " offset: " << it->offset
                  << " length: " << it->segment.length();
      stream_->Reset(QUIC_ERROR_PROCESSING_STREAM);
      return;
    }

    if (it->offset + it->segment.length() <= end_offset) {
      RecordBytesConsumed(it->segment.length());
      // This chunk is entirely consumed.
      buffered_frames_.erase(it);
      continue;
    }

    // Partially consume this frame.
    size_t delta = end_offset - it->offset;
    RecordBytesConsumed(delta);
    string new_data = it->segment.substr(delta);
    buffered_frames_.erase(it);
    buffered_frames_.push_front(FrameData(num_bytes_consumed_, new_data));
    break;
  }
}

bool QuicStreamSequencer::IsDuplicate(
    const QuicStreamFrame& frame,
    FrameList::const_iterator insertion_point) const {
  // A frame is duplicate if the frame offset is smaller than the bytes consumed
  // or identical to an already received frame.
  return frame.offset < num_bytes_consumed_ ||
         (insertion_point != buffered_frames_.end() &&
          frame.offset == insertion_point->offset);
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
