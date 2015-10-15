// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FRAME_LIST_H_
#define NET_QUIC_QUIC_FRAME_LIST_H_

#include "net/quic/quic_frame_list.h"
#include "net/quic/quic_protocol.h"

using base::StringPiece;
using std::string;
using std::list;

namespace net {

namespace test {
class QuicStreamSequencerPeer;
}

class NET_EXPORT_PRIVATE QuicFrameList {
 public:
  // A contiguous segment received by a QUIC stream.
  struct FrameData {
    FrameData(QuicStreamOffset offset, string segment);

    const QuicStreamOffset offset;
    string segment;
  };

  explicit QuicFrameList();

  ~QuicFrameList();

  // Clear the buffer such that it is in its initial, newly constructed state.
  void Clear() { frame_list_.clear(); }

  // Returns true if there is nothing to read in this buffer.
  bool Empty() const { return frame_list_.empty(); }

  // Write the supplied data to this buffer. If the write was successful,
  // return the number of bytes written in |bytes_written|.
  // Return QUIC_INVALID_STREAM_DATA if |data| overlaps with existing data.
  // No data will be written.
  // Return QUIC_NO_ERROR, if |data| is duplicated with data written previously,
  // and |bytes_written| = 0
  QuicErrorCode WriteAtOffset(QuicStreamOffset offset,
                              StringPiece data,
                              size_t* bytes_written);

  // Read from this buffer into given iovec array, upto number of iov_len iovec
  // objects.
  // Returns the number of bytes read into iov.
  size_t ReadvAndInvalidate(const struct iovec* iov, size_t iov_len);

  // Returns the readable region of valid data in iovec format. The readable
  // region is the buffer region where there is valid data not yet read by
  // client. ReadAndInvalidate() and WriteAtOffset() change the readable region.
  // The return value of this function is the number of iovec entries
  // filled into in iov. If the region is empty, one iovec entry with 0 length
  // is returned, and the function returns 0. If there are more readable
  // regions than iov_size, the function only processes the first
  // iov_size of them.
  int GetReadableRegions(struct iovec* iov, int iov_len) const;

  // Called after GetReadableRegions() to accumulate total_bytes_read_ and free
  // up block when all data in it have been read out.
  // Pre-requisite: bytes_used <= ReadableBytes()
  bool IncreaseTotalReadAndInvalidate(size_t bytes_used);

  // Whether there are bytes can be read out (offset == total_bytes_read_)
  bool HasBytesToRead() const;

  size_t size() const { return frame_list_.size(); }

  QuicStreamOffset total_bytes_read() const { return total_bytes_read_; }

 private:
  friend class test::QuicStreamSequencerPeer;

  list<FrameData>::iterator FindInsertionPoint(QuicStreamOffset offset,
                                               size_t len);

  bool FrameOverlapsBufferedData(
      QuicStreamOffset offset,
      size_t data_len,
      list<FrameData>::const_iterator insertion_point) const;

  bool IsDuplicate(QuicStreamOffset offset,
                   size_t data_len,
                   list<FrameData>::const_iterator insertion_point) const;

  list<FrameData> frame_list_;
  QuicStreamOffset total_bytes_read_ = 0;
};

}  // namespace net_quic

#endif  // NET_QUIC_QUIC_FRAME_LIST_H_
