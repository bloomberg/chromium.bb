// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server reliable streams.

#ifndef NET_QUIC_RELIABLE_QUIC_STREAM_H_
#define NET_QUIC_RELIABLE_QUIC_STREAM_H_

#include <sys/types.h>

#include <list>

#include "base/strings/string_piece.h"
#include "net/base/iovec.h"
#include "net/base/net_export.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_stream_sequencer.h"

namespace net {

namespace test {
class ReliableQuicStreamPeer;
}  // namespace test

class IPEndPoint;
class QuicSession;
class SSLInfo;

// All this does right now is send data to subclasses via the sequencer.
class NET_EXPORT_PRIVATE ReliableQuicStream : public
    QuicSpdyDecompressor::Visitor {
 public:
  // Visitor receives callbacks from the stream.
  class Visitor {
   public:
    Visitor() {}

    // Called when the stream is closed.
    virtual void OnClose(ReliableQuicStream* stream) = 0;

   protected:
    virtual ~Visitor() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Visitor);
  };

  ReliableQuicStream(QuicStreamId id,
                     QuicSession* session);

  virtual ~ReliableQuicStream();

  bool WillAcceptStreamFrame(const QuicStreamFrame& frame) const;
  virtual bool OnStreamFrame(const QuicStreamFrame& frame);

  virtual void OnCanWrite();

  // Called by the session just before the stream is deleted.
  virtual void OnClose();

  // Called when we get a stream reset from the client.
  virtual void OnStreamReset(QuicRstStreamErrorCode error);

  // Called when we get or send a connection close, and should immediately
  // close the stream.  This is not passed through the sequencer,
  // but is handled immediately.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer);

  // Called when we should process a stream termination or
  // stream close from the peer.
  virtual void TerminateFromPeer(bool half_close);

  virtual uint32 ProcessRawData(const char* data, uint32 data_len);
  virtual uint32 ProcessHeaderData();

  virtual uint32 ProcessData(const char* data, uint32 data_len) = 0;

  virtual bool OnDecompressedData(base::StringPiece data) OVERRIDE;
  virtual void OnDecompressionError() OVERRIDE;

  // Called to close the stream from this end.
  virtual void Close(QuicRstStreamErrorCode error);

  // This block of functions wraps the sequencer's functions of the same
  // name.  These methods return uncompressed data until that has
  // been fully processed.  Then they simply delegate to the sequencer.
  virtual size_t Readv(const struct iovec* iov, size_t iov_len);
  virtual int GetReadableRegions(iovec* iov, size_t iov_len);
  virtual bool IsHalfClosed() const;
  virtual bool HasBytesToRead() const;

  // Called by the session when a decompression blocked stream
  // becomes unblocked.
  virtual void OnDecompressorAvailable();

  QuicStreamId id() const { return id_; }

  QuicRstStreamErrorCode stream_error() const { return stream_error_; }
  QuicErrorCode connection_error() const { return connection_error_; }

  bool read_side_closed() const { return read_side_closed_; }
  bool write_side_closed() const { return write_side_closed_; }

  uint64 stream_bytes_read() { return stream_bytes_read_; }
  uint64 stream_bytes_written() { return stream_bytes_written_; }

  const IPEndPoint& GetPeerAddress() const;

  Visitor* visitor() { return visitor_; }
  void set_visitor(Visitor* visitor) { visitor_ = visitor; }

  QuicSpdyCompressor* compressor();

  // Gets the SSL connection information.
  bool GetSSLInfo(SSLInfo* ssl_info);

  bool headers_decompressed() const { return headers_decompressed_; }

 protected:
  // Returns a pair with the number of bytes consumed from data, and a boolean
  // indicating if the fin bit was consumed.  This does not indicate the data
  // has been sent on the wire: it may have been turned into a packet and queued
  // if the socket was unexpectedly blocked.
  //
  // The default implementation always consumed all bytes and any fin, but
  // this behavior is not guaranteed for subclasses so callers should check the
  // return value.
  virtual QuicConsumedData WriteData(base::StringPiece data, bool fin);

  // Close the read side of the socket.  Further frames will not be accepted.
  virtual void CloseReadSide();

  // Close the write side of the socket.  Further writes will fail.
  void CloseWriteSide();

  bool fin_buffered() { return fin_buffered_; }

  QuicSession* session() { return session_; }

  // Sends as much of 'data' to the connection as the connection will consume,
  // and then buffers any remaining data in queued_data_.
  // Returns (data.size(), true) as it always consumed all data: it returns for
  // convenience to have the same return type as WriteDataInternal.
  QuicConsumedData WriteOrBuffer(base::StringPiece data, bool fin);

  // Sends as much of 'data' to the connection as the connection will consume.
  // Returns the number of bytes consumed by the connection.
  QuicConsumedData WriteDataInternal(base::StringPiece data, bool fin);

 private:
  friend class test::ReliableQuicStreamPeer;
  friend class QuicStreamUtils;

  std::list<string> queued_data_;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  QuicSession* session_;
  // Optional visitor of this stream to be notified when the stream is closed.
  Visitor* visitor_;
  // Bytes read and written refer to payload bytes only: they do not include
  // framing, encryption overhead etc.
  uint64 stream_bytes_read_;
  uint64 stream_bytes_written_;
  // True if the headers have been completely decompresssed.
  bool headers_decompressed_;
  // ID of the header block sent by the peer, once parsed.
  QuicHeaderId headers_id_;
  // Buffer into which we write bytes from the headers_id_
  // until it is fully parsed.
  string headers_id_buffer_;
  // Contains a copy of the decompressed headers_ until they are consumed
  // via ProcessData or Readv.
  string decompressed_headers_;
  // True if an error was encountered during decompression.
  bool decompression_failed_;

  // Stream error code received from a RstStreamFrame or error code sent by the
  // visitor or sequencer in the RstStreamFrame.
  QuicRstStreamErrorCode stream_error_;
  // Connection error code due to which the stream was closed. |stream_error_|
  // is set to |QUIC_STREAM_CONNECTION_ERROR| when this happens and consumers
  // should check |connection_error_|.
  QuicErrorCode connection_error_;

  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;

  bool fin_buffered_;
  bool fin_sent_;
};

}  // namespace net

#endif  // NET_QUIC_RELIABLE_QUIC_STREAM_H_
