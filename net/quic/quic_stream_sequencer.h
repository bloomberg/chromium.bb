// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_SEQUENCER_H_
#define NET_QUIC_QUIC_STREAM_SEQUENCER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "net/quic/quic_frame_list.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace test {
class QuicStreamSequencerPeer;
}  // namespace test

class QuicSession;
class ReliableQuicStream;

// Buffers frames until we have something which can be passed
// up to the next layer.
class NET_EXPORT_PRIVATE QuicStreamSequencer {
 public:
  explicit QuicStreamSequencer(ReliableQuicStream* quic_stream);
  virtual ~QuicStreamSequencer();

  // If the frame is the next one we need in order to process in-order data,
  // ProcessData will be immediately called on the stream until all buffered
  // data is processed or the stream fails to consume data.  Any unconsumed
  // data will be buffered. If the frame is not the next in line, it will be
  // buffered.
  void OnStreamFrame(const QuicStreamFrame& frame);

  // Once data is buffered, it's up to the stream to read it when the stream
  // can handle more data.  The following three functions make that possible.

  // Fills in up to iov_len iovecs with the next readable regions.  Returns the
  // number of iovs used.  Non-destructive of the underlying data.
  int GetReadableRegions(iovec* iov, size_t iov_len) const;

  // Copies the data into the iov_len buffers provided.  Returns the number of
  // bytes read.  Any buffered data no longer in use will be released.
  // TODO(rch): remove this method and instead implement it as a helper method
  // based on GetReadableRegions and MarkConsumed.
  int Readv(const struct iovec* iov, size_t iov_len);

  // Consumes |num_bytes| data.  Used in conjunction with |GetReadableRegions|
  // to do zero-copy reads.
  void MarkConsumed(size_t num_bytes);

  // Returns true if the sequncer has bytes available for reading.
  bool HasBytesToRead() const;

  // Returns true if the sequencer has delivered the fin.
  bool IsClosed() const;

  // Calls |OnDataAvailable| on |stream_| if there is buffered data that can
  // be processed, and causes |OnDataAvailable| to be called as new data
  // arrives.
  void SetUnblocked();

  // Blocks processing of frames until |SetUnblocked| is called.
  void SetBlockedUntilFlush();

  size_t num_bytes_buffered() const { return num_bytes_buffered_; }
  QuicStreamOffset num_bytes_consumed() const { return num_bytes_consumed_; }

  int num_frames_received() const { return num_frames_received_; }

  int num_duplicate_frames_received() const {
    return num_duplicate_frames_received_;
  }

  int num_early_frames_received() const { return num_early_frames_received_; }

 private:
  friend class test::QuicStreamSequencerPeer;

  // Wait until we've seen 'offset' bytes, and then terminate the stream.
  void CloseStreamAtOffset(QuicStreamOffset offset);

  // If we've received a FIN and have processed all remaining data, then inform
  // the stream of FIN, and clear buffers.
  bool MaybeCloseStream();

  // Called whenever bytes are consumed by the stream. Updates
  // num_bytes_consumed_ and num_bytes_buffered_.
  void RecordBytesConsumed(size_t bytes_consumed);

  // The stream which owns this sequencer.
  ReliableQuicStream* stream_;

  // The last data consumed by the stream.
  QuicStreamOffset num_bytes_consumed_;

  // Stores buffered frames in offset order.
  QuicFrameList buffered_frames_;

  // The offset, if any, we got a stream termination for.  When this many bytes
  // have been processed, the sequencer will be closed.
  QuicStreamOffset close_offset_;

  // If true, the sequencer is blocked from passing data to the stream and will
  // buffer all new incoming data until FlushBufferedFrames is called.
  bool blocked_;

  // Tracks how many bytes the sequencer has buffered.
  size_t num_bytes_buffered_;

  // Count of the number of frames received.
  int num_frames_received_;

  // Count of the number of duplicate frames received.
  int num_duplicate_frames_received_;

  // Count of the number of frames received before all previous frames were
  // received.
  int num_early_frames_received_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamSequencer);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_SEQUENCER_H_
