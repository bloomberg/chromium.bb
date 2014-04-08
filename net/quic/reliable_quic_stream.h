// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server reliable streams.

#ifndef NET_QUIC_RELIABLE_QUIC_STREAM_H_
#define NET_QUIC_RELIABLE_QUIC_STREAM_H_

#include <sys/types.h>

#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/iovec.h"
#include "net/base/net_export.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_stream_sequencer.h"

namespace net {

namespace test {
class ReliableQuicStreamPeer;
}  // namespace test

class QuicSession;

class NET_EXPORT_PRIVATE ReliableQuicStream {
 public:
  ReliableQuicStream(QuicStreamId id,
                     QuicSession* session);

  virtual ~ReliableQuicStream();

  bool WillAcceptStreamFrame(const QuicStreamFrame& frame) const;

  // Called when a (potentially duplicate) stream frame has been received
  // for this stream.  Returns false if this frame can not be accepted
  // because there is too much data already buffered.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame);

  // Called when the connection becomes writeable to allow the stream
  // to write any pending data.
  virtual void OnCanWrite();

  // Called by the session just before the stream is deleted.
  virtual void OnClose();

  // Called when we get a stream reset from the peer.
  virtual void OnStreamReset(const QuicRstStreamFrame& frame);

  // Called when we get or send a connection close, and should immediately
  // close the stream.  This is not passed through the sequencer,
  // but is handled immediately.
  virtual void OnConnectionClosed(QuicErrorCode error, bool from_peer);

  // Called when the final data has been read.
  virtual void OnFinRead();

  virtual uint32 ProcessRawData(const char* data, uint32 data_len) = 0;

  // Called to reset the stream from this end.
  virtual void Reset(QuicRstStreamErrorCode error);

  // Called to close the entire connection from this end.
  virtual void CloseConnection(QuicErrorCode error);
  virtual void CloseConnectionWithDetails(QuicErrorCode error,
                                          const string& details);

  // Returns the effective priority for the stream.  This value may change
  // during the life of the stream.
  virtual QuicPriority EffectivePriority() const = 0;

  QuicStreamId id() const { return id_; }

  QuicRstStreamErrorCode stream_error() const { return stream_error_; }
  QuicErrorCode connection_error() const { return connection_error_; }

  bool read_side_closed() const { return read_side_closed_; }
  bool write_side_closed() const { return write_side_closed_; }

  uint64 stream_bytes_read() const { return stream_bytes_read_; }
  uint64 stream_bytes_written() const { return stream_bytes_written_; }

  QuicVersion version() const;

  void set_fin_sent(bool fin_sent) { fin_sent_ = fin_sent; }
  void set_rst_sent(bool rst_sent) { rst_sent_ = rst_sent; }

  // Adjust our flow control windows according to new offset in |frame|.
  virtual void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame);

  // True if this stream is blocked from writing due to flow control limits.
  bool IsFlowControlBlocked() const;

  // Updates our send window offset (if offset larger).
  void UpdateFlowControlSendLimit(QuicStreamOffset offset);

  // If our receive window has dropped below the threshold, then send a
  // WINDOW_UPDATE frame. This is called whenever bytes are consumed from the
  // sequencer's buffer.
  void MaybeSendWindowUpdate();

  int num_frames_received();

  int num_duplicate_frames_received();

 protected:
  // Sends as much of 'data' to the connection as the connection will consume,
  // and then buffers any remaining data in queued_data_.
  void WriteOrBufferData(
      base::StringPiece data,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate);

  // Sends as many bytes in the first |count| buffers of |iov| to the connection
  // as the connection will consume.
  // If |ack_notifier_delegate| is provided, then it will be notified once all
  // the ACKs for this write have been received.
  // Returns the number of bytes consumed by the connection.
  QuicConsumedData WritevData(
      const struct iovec* iov,
      int iov_count,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate);

  // Close the read side of the socket.  Further frames will not be accepted.
  virtual void CloseReadSide();

  // Close the write side of the socket.  Further writes will fail.
  void CloseWriteSide();

  bool HasBufferedData();

  bool fin_buffered() { return fin_buffered_; }

  const QuicSession* session() const { return session_; }
  QuicSession* session() { return session_; }

  const QuicStreamSequencer* sequencer() const { return &sequencer_; }
  QuicStreamSequencer* sequencer() { return &sequencer_; }

  // Returns true if flow control is enabled for this stream.
  virtual bool IsFlowControlEnabled() const = 0;

 private:
  friend class test::ReliableQuicStreamPeer;
  friend class QuicStreamUtils;
  class ProxyAckNotifierDelegate;

  struct PendingData {
    PendingData(string data_in,
                scoped_refptr<ProxyAckNotifierDelegate> delegate_in);
    ~PendingData();

    string data;
    // Delegate that should be notified when the pending data is acked.
    // Can be nullptr.
    scoped_refptr<ProxyAckNotifierDelegate> delegate;
  };

  // Calculates and returns available flow control send window.
  uint64 SendWindowSize() const;

  // Calculates and returns total number of bytes this stream has received.
  uint64 TotalReceivedBytes() const;

  std::list<PendingData> queued_data_;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  QuicSession* session_;
  // Bytes read and written refer to payload bytes only: they do not include
  // framing, encryption overhead etc.
  uint64 stream_bytes_read_;
  uint64 stream_bytes_written_;

  // Stream error code received from a RstStreamFrame or error code sent by the
  // visitor or sequencer in the RstStreamFrame.
  QuicRstStreamErrorCode stream_error_;
  // Connection error code due to which the stream was closed. |stream_error_|
  // is set to |QUIC_STREAM_CONNECTION_ERROR| when this happens and consumers
  // should check |connection_error_|.
  QuicErrorCode connection_error_;

  // Stream level flow control.
  // This stream is allowed to send up to flow_control_send_limit_ bytes. Once
  // it has reached this limit it must not send more data until it receives a
  // suitable WINDOW_UPDATE frame from the peer.
  QuicStreamOffset flow_control_send_limit_;

  // Stream level flow control.
  // The maximum size of the stream receive window. Used to determine by how
  // much we should increase the window offset when sending a WINDOW_UPDATE.
  uint64 max_flow_control_receive_window_bytes_;

  // Stream level flow control.
  // This stream expects to receive up to receive_window_offset_bytes_.
  // If the peer sends more than this (without sending us a WINDOW_UPDATE frame
  // first), then this is a flow control error.
  QuicStreamOffset flow_control_receive_window_offset_bytes_;

  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;

  bool fin_buffered_;
  bool fin_sent_;

  // In combination with fin_sent_, used to ensure that a FIN and/or a RST is
  // always sent before stream termination.
  bool rst_sent_;

  // True if the session this stream is running under is a server session.
  bool is_server_;

  DISALLOW_COPY_AND_ASSIGN(ReliableQuicStream);
};

}  // namespace net

#endif  // NET_QUIC_RELIABLE_QUIC_STREAM_H_
