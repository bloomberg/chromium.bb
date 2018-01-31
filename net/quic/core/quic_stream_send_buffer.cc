// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/core/quic_data_writer.h"
#include "net/quic/core/quic_stream_send_buffer.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/platform/api/quic_bug_tracker.h"
#include "net/quic/platform/api/quic_flag_utils.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_logging.h"

namespace net {

BufferedSlice::BufferedSlice(QuicMemSlice mem_slice, QuicStreamOffset offset)
    : slice(std::move(mem_slice)), offset(offset) {}

BufferedSlice::BufferedSlice(BufferedSlice&& other) = default;

BufferedSlice& BufferedSlice::operator=(BufferedSlice&& other) = default;

BufferedSlice::~BufferedSlice() {}

bool StreamPendingRetransmission::operator==(
    const StreamPendingRetransmission& other) const {
  return offset == other.offset && length == other.length;
}

QuicStreamSendBuffer::QuicStreamSendBuffer(QuicBufferAllocator* allocator)
    : stream_offset_(0),
      allocator_(allocator),
      stream_bytes_written_(0),
      stream_bytes_outstanding_(0),
      write_index_(-1),
      use_write_index_(GetQuicReloadableFlag(quic_use_write_index)) {}

QuicStreamSendBuffer::~QuicStreamSendBuffer() {}

void QuicStreamSendBuffer::SaveStreamData(const struct iovec* iov,
                                          int iov_count,
                                          size_t iov_offset,
                                          QuicByteCount data_length) {
  DCHECK_LT(0u, data_length);
  // Latch the maximum data slice size.
  const QuicByteCount max_data_slice_size =
      GetQuicFlag(FLAGS_quic_send_buffer_max_data_slice_size);
  while (data_length > 0) {
    size_t slice_len = std::min(data_length, max_data_slice_size);
    QuicMemSlice slice(allocator_, slice_len);
    QuicUtils::CopyToBuffer(iov, iov_count, iov_offset, slice_len,
                            const_cast<char*>(slice.data()));
    SaveMemSlice(std::move(slice));
    data_length -= slice_len;
    iov_offset += slice_len;
  }
}

void QuicStreamSendBuffer::SaveMemSlice(QuicMemSlice slice) {
  QUIC_DVLOG(2) << "Save slice offset " << stream_offset_ << " length "
                << slice.length();
  if (slice.empty()) {
    QUIC_BUG << "Try to save empty MemSlice to send buffer.";
    return;
  }
  size_t length = slice.length();
  buffered_slices_.emplace_back(std::move(slice), stream_offset_);
  if (write_index_ == -1) {
    write_index_ = buffered_slices_.size() - 1;
  }
  stream_offset_ += length;
}

void QuicStreamSendBuffer::OnStreamDataConsumed(size_t bytes_consumed) {
  stream_bytes_written_ += bytes_consumed;
  stream_bytes_outstanding_ += bytes_consumed;
}

bool QuicStreamSendBuffer::WriteStreamData(QuicStreamOffset offset,
                                           QuicByteCount data_length,
                                           QuicDataWriter* writer) {
  if (use_write_index_) {
    return WriteStreamDataWithIndex(offset, data_length, writer);
  }
  for (const BufferedSlice& slice : buffered_slices_) {
    if (data_length == 0 || offset < slice.offset) {
      break;
    }
    if (offset >= slice.offset + slice.slice.length()) {
      continue;
    }
    QuicByteCount slice_offset = offset - slice.offset;
    QuicByteCount copy_length =
        std::min(data_length, slice.slice.length() - slice_offset);
    if (!writer->WriteBytes(slice.slice.data() + slice_offset, copy_length)) {
      return false;
    }
    offset += copy_length;
    data_length -= copy_length;
  }

  return data_length == 0;
}

bool QuicStreamSendBuffer::WriteStreamDataWithIndex(QuicStreamOffset offset,
                                                    QuicByteCount data_length,
                                                    QuicDataWriter* writer) {
  bool write_index_hit = false;
  QuicDeque<BufferedSlice>::iterator slice_it =
      write_index_ == -1
          ? buffered_slices_.begin()
          // Assume with write_index, write mostly starts from indexed slice.
          : buffered_slices_.begin() + write_index_;
  if (write_index_ != -1) {
    if (offset >= slice_it->offset + slice_it->slice.length()) {
      QUIC_BUG << "Tried to write data out of sequence.";
      return false;
    }
    // Determine if write actually happens at indexed slice.
    if (offset >= slice_it->offset) {
      write_index_hit = true;
      QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_use_write_index, 1, 2);
    } else {
      // Write index missed, move iterator to the beginning.
      slice_it = buffered_slices_.begin();
    }
  }

  for (; slice_it != buffered_slices_.end(); ++slice_it) {
    if (data_length == 0 || offset < slice_it->offset) {
      break;
    }
    if (offset >= slice_it->offset + slice_it->slice.length()) {
      continue;
    }
    QuicByteCount slice_offset = offset - slice_it->offset;
    QuicByteCount available_bytes_in_slice =
        slice_it->slice.length() - slice_offset;
    QuicByteCount copy_length = std::min(data_length, available_bytes_in_slice);
    if (!writer->WriteBytes(slice_it->slice.data() + slice_offset,
                            copy_length)) {
      QUIC_BUG << "Writer fails to write.";
      return false;
    }
    offset += copy_length;
    data_length -= copy_length;

    if (write_index_hit && copy_length == available_bytes_in_slice) {
      // Finished writing all data in current slice, advance write index for
      // next write.
      ++write_index_;
    }
  }

  if (write_index_hit &&
      static_cast<size_t>(write_index_) == buffered_slices_.size()) {
    // Already write to the end off buffer.
    DVLOG(2) << "Finish writing out all buffered data.";
    write_index_ = -1;
  }

  return data_length == 0;
}

bool QuicStreamSendBuffer::OnStreamDataAcked(
    QuicStreamOffset offset,
    QuicByteCount data_length,
    QuicByteCount* newly_acked_length) {
  *newly_acked_length = 0;
  if (data_length == 0) {
    return true;
  }
  QuicIntervalSet<QuicStreamOffset> newly_acked(offset, offset + data_length);
  newly_acked.Difference(bytes_acked_);
  for (const auto& interval : newly_acked) {
    *newly_acked_length += (interval.max() - interval.min());
  }
  if (stream_bytes_outstanding_ < *newly_acked_length) {
    return false;
  }
  stream_bytes_outstanding_ -= *newly_acked_length;
  bytes_acked_.Add(offset, offset + data_length);
  pending_retransmissions_.Difference(offset, offset + data_length);
  while (!buffered_slices_.empty() &&
         bytes_acked_.Contains(buffered_slices_.front().offset,
                               buffered_slices_.front().offset +
                                   buffered_slices_.front().slice.length())) {
    // Remove data which stops waiting for acks. Please note, data can be
    // acked out of order, but send buffer is cleaned up in order.
    if (use_write_index_) {
      QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_use_write_index, 2, 2);
      QUIC_BUG_IF(write_index_ == 0)
          << "Fail to advance current_write_slice_. It points to the slice "
             "whose data has all be written and ACK'ed or ignored. "
             "current_write_slice_ offset "
          << buffered_slices_[write_index_].offset << " length "
          << buffered_slices_[write_index_].slice.length();
      if (write_index_ > 0) {
        // If write index is pointing to any slice, reduce the index as the
        // slices are all shifted to the left by one.
        --write_index_;
      }
    }
    buffered_slices_.pop_front();
  }
  return true;
}

void QuicStreamSendBuffer::OnStreamDataLost(QuicStreamOffset offset,
                                            QuicByteCount data_length) {
  if (data_length == 0) {
    return;
  }
  QuicIntervalSet<QuicStreamOffset> bytes_lost(offset, offset + data_length);
  bytes_lost.Difference(bytes_acked_);
  if (bytes_lost.Empty()) {
    return;
  }
  for (const auto& lost : bytes_lost) {
    pending_retransmissions_.Add(lost.min(), lost.max());
  }
}

void QuicStreamSendBuffer::OnStreamDataRetransmitted(
    QuicStreamOffset offset,
    QuicByteCount data_length) {
  if (data_length == 0) {
    return;
  }
  pending_retransmissions_.Difference(offset, offset + data_length);
}

bool QuicStreamSendBuffer::HasPendingRetransmission() const {
  return !pending_retransmissions_.Empty();
}

StreamPendingRetransmission QuicStreamSendBuffer::NextPendingRetransmission()
    const {
  if (HasPendingRetransmission()) {
    const auto pending = pending_retransmissions_.begin();
    return {pending->min(), pending->max() - pending->min()};
  }
  QUIC_BUG << "NextPendingRetransmission is called unexpected with no "
              "pending retransmissions.";
  return {0, 0};
}

bool QuicStreamSendBuffer::IsStreamDataOutstanding(
    QuicStreamOffset offset,
    QuicByteCount data_length) const {
  return data_length > 0 &&
         !bytes_acked_.Contains(offset, offset + data_length);
}

size_t QuicStreamSendBuffer::size() const {
  return buffered_slices_.size();
}

}  // namespace net
