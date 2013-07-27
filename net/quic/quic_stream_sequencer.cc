// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "net/quic/reliable_quic_stream.h"

using std::min;
using std::numeric_limits;

namespace net {

QuicStreamSequencer::QuicStreamSequencer(ReliableQuicStream* quic_stream)
    : stream_(quic_stream),
      num_bytes_consumed_(0),
      max_frame_memory_(numeric_limits<size_t>::max()),
      close_offset_(numeric_limits<QuicStreamOffset>::max()) {
}

QuicStreamSequencer::QuicStreamSequencer(size_t max_frame_memory,
                                         ReliableQuicStream* quic_stream)
    : stream_(quic_stream),
      num_bytes_consumed_(0),
      max_frame_memory_(max_frame_memory),
      close_offset_(numeric_limits<QuicStreamOffset>::max()) {
  if (max_frame_memory < kMaxPacketSize) {
    LOG(DFATAL) << "Setting max frame memory to " << max_frame_memory
                << ".  Some frames will be impossible to handle.";
  }
}

QuicStreamSequencer::~QuicStreamSequencer() {
}

bool QuicStreamSequencer::WillAcceptStreamFrame(
    const QuicStreamFrame& frame) const {
  size_t data_len = frame.data.size();
  DCHECK_LE(data_len, max_frame_memory_);

  if (IsDuplicate(frame)) {
    return true;
  }
  QuicStreamOffset byte_offset = frame.offset;
  if (data_len > max_frame_memory_) {
    // We're never going to buffer this frame and we can't pass it up.
    // The stream might only consume part of it and we'd need a partial ack.
    //
    // Ideally this should never happen, as we check that
    // max_frame_memory_ > kMaxPacketSize and lower levels should reject
    // frames larger than that.
    return false;
  }
  if (byte_offset + data_len - num_bytes_consumed_ > max_frame_memory_) {
    // We can buffer this but not right now.  Toss it.
    // It might be worth trying an experiment where we try best-effort buffering
    return false;
  }
  return true;
}

bool QuicStreamSequencer::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!WillAcceptStreamFrame(frame)) {
    // This should not happen, as WillAcceptFrame should be called before
    // OnStreamFrame.  Error handling should be done by the caller.
    return false;
  }
  if (IsDuplicate(frame)) {
    // Silently ignore duplicates.
    return true;
  }

  if (frame.fin) {
    CloseStreamAtOffset(frame.offset + frame.data.size());
  }

  QuicStreamOffset byte_offset = frame.offset;
  const char* data = frame.data.data();
  size_t data_len = frame.data.size();

  if (data_len == 0) {
    // TODO(rch): Close the stream if there was no data and no fin.
    return true;
  }

  if (byte_offset == num_bytes_consumed_) {
    DVLOG(1) << "Processing byte offset " << byte_offset;
    size_t bytes_consumed = stream_->ProcessRawData(data, data_len);
    num_bytes_consumed_ += bytes_consumed;

    if (MaybeCloseStream()) {
      return true;
    }
    if (bytes_consumed > data_len) {
      stream_->Close(QUIC_SERVER_ERROR_PROCESSING_STREAM);
      return false;
    } else if (bytes_consumed == data_len) {
      FlushBufferedFrames();
      return true;  // it's safe to ack this frame.
    } else {
      // Set ourselves up to buffer what's left
      data_len -= bytes_consumed;
      data += bytes_consumed;
      byte_offset += bytes_consumed;
    }
  }
  DVLOG(1) << "Buffering packet at offset " << byte_offset;
  frames_.insert(make_pair(byte_offset, string(data, data_len)));
  return true;
}

void QuicStreamSequencer::CloseStreamAtOffset(QuicStreamOffset offset) {
  const QuicStreamOffset kMaxOffset = numeric_limits<QuicStreamOffset>::max();

  // If we have a scheduled termination or close, any new offset should match
  // it.
  if (close_offset_ != kMaxOffset && offset != close_offset_) {
    stream_->Close(QUIC_MULTIPLE_TERMINATION_OFFSETS);
    return;
  }

  close_offset_ = offset;

  MaybeCloseStream();
}

bool QuicStreamSequencer::MaybeCloseStream() {
  if (IsHalfClosed()) {
    DVLOG(1) << "Passing up termination, as we've processed "
             << num_bytes_consumed_ << " of " << close_offset_
             << " bytes.";
    // Technically it's an error if num_bytes_consumed isn't exactly
    // equal, but error handling seems silly at this point.
    stream_->TerminateFromPeer(true);
    return true;
  }
  return false;
}

int QuicStreamSequencer::GetReadableRegions(iovec* iov, size_t iov_len) {
  FrameMap::iterator it = frames_.begin();
  size_t index = 0;
  QuicStreamOffset offset = num_bytes_consumed_;
  while (it != frames_.end() && index < iov_len) {
    if (it->first != offset) return index;

    iov[index].iov_base = static_cast<void*>(
        const_cast<char*>(it->second.data()));
    iov[index].iov_len = it->second.size();
    offset += it->second.size();

    ++index;
    ++it;
  }
  return index;
}

int QuicStreamSequencer::Readv(const struct iovec* iov, size_t iov_len) {
  FrameMap::iterator it = frames_.begin();
  size_t iov_index = 0;
  size_t iov_offset = 0;
  size_t frame_offset = 0;
  size_t initial_bytes_consumed = num_bytes_consumed_;

  while (iov_index < iov_len &&
         it != frames_.end() &&
         it->first == num_bytes_consumed_) {
    int bytes_to_read = min(iov[iov_index].iov_len - iov_offset,
                            it->second.size() - frame_offset);

    char* iov_ptr = static_cast<char*>(iov[iov_index].iov_base) + iov_offset;
    memcpy(iov_ptr,
           it->second.data() + frame_offset, bytes_to_read);
    frame_offset += bytes_to_read;
    iov_offset += bytes_to_read;

    if (iov[iov_index].iov_len == iov_offset) {
      // We've filled this buffer.
      iov_offset = 0;
      ++iov_index;
    }
    if (it->second.size() == frame_offset) {
      // We've copied this whole frame
      num_bytes_consumed_ += it->second.size();
      frames_.erase(it);
      it = frames_.begin();
      frame_offset = 0;
    }
  }
  // We've finished copying.  If we have a partial frame, update it.
  if (frame_offset != 0) {
    frames_.insert(make_pair(it->first + frame_offset,
                             it->second.substr(frame_offset)));
    frames_.erase(frames_.begin());
    num_bytes_consumed_ += frame_offset;
  }
  return num_bytes_consumed_ - initial_bytes_consumed;
}

void QuicStreamSequencer::MarkConsumed(size_t num_bytes_consumed) {
  size_t end_offset = num_bytes_consumed_ + num_bytes_consumed;
  while (!frames_.empty() && end_offset != num_bytes_consumed_) {
    FrameMap::iterator it = frames_.begin();
    if (it->first != num_bytes_consumed_) {
      LOG(DFATAL) << "Invalid argument to MarkConsumed. "
                  << " num_bytes_consumed_: " << num_bytes_consumed_
                  << " end_offset: " << end_offset
                  << " offset: " << it->first
                  << " length: " << it->second.length();
      stream_->Close(QUIC_SERVER_ERROR_PROCESSING_STREAM);
      return;
    }

    if (it->first + it->second.length() <= end_offset) {
      num_bytes_consumed_ += it->second.length();
      // This chunk is entirely consumed.
      frames_.erase(it);
      continue;
    }

    // Partially consume this frame.
    size_t delta = end_offset - it->first;
    num_bytes_consumed_ += delta;
    frames_.insert(make_pair(end_offset, it->second.substr(delta)));
    frames_.erase(it);
    break;
  }
}

bool QuicStreamSequencer::HasBytesToRead() const {
  FrameMap::const_iterator it = frames_.begin();

  return it != frames_.end() && it->first == num_bytes_consumed_;
}

bool QuicStreamSequencer::IsHalfClosed() const {
  return num_bytes_consumed_ >= close_offset_;
}

bool QuicStreamSequencer::IsDuplicate(const QuicStreamFrame& frame) const {
  // A frame is duplicate if the frame offset is smaller than our bytes consumed
  // or we have stored the frame in our map.
  // TODO(pwestin): Is it possible that a new frame contain more data even if
  // the offset is the same?
  return frame.offset < num_bytes_consumed_ ||
      frames_.find(frame.offset) != frames_.end();
}

void QuicStreamSequencer::FlushBufferedFrames() {
  FrameMap::iterator it = frames_.find(num_bytes_consumed_);
  while (it != frames_.end()) {
    DVLOG(1) << "Flushing buffered packet at offset " << it->first;
    string* data = &it->second;
    size_t bytes_consumed = stream_->ProcessRawData(data->c_str(),
                                                    data->size());
    num_bytes_consumed_ += bytes_consumed;
    if (MaybeCloseStream()) {
      return;
    }
    if (bytes_consumed > data->size()) {
      stream_->Close(QUIC_SERVER_ERROR_PROCESSING_STREAM);  // Programming error
      return;
    } else if (bytes_consumed == data->size()) {
      frames_.erase(it);
      it = frames_.find(num_bytes_consumed_);
    } else {
      string new_data = it->second.substr(bytes_consumed);
      frames_.erase(it);
      frames_.insert(make_pair(num_bytes_consumed_, new_data));
      return;
    }
  }
}

}  // namespace net
