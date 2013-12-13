// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for streams which deliver data to/from an application.
// In each direction, the data on such a stream first contains compressed
// headers then body data.

#ifndef NET_QUIC_QUIC_DATA_STREAM_H_
#define NET_QUIC_QUIC_DATA_STREAM_H_

#include <sys/types.h>

#include <list>

#include "base/strings/string_piece.h"
#include "net/base/iovec.h"
#include "net/base/net_export.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_stream_sequencer.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

namespace test {
class QuicDataStreamPeer;
class ReliableQuicStreamPeer;
}  // namespace test

class IPEndPoint;
class QuicSession;
class SSLInfo;

// All this does right now is send data to subclasses via the sequencer.
class NET_EXPORT_PRIVATE QuicDataStream : public ReliableQuicStream,
                                          public QuicSpdyDecompressor::Visitor {
 public:
  // Visitor receives callbacks from the stream.
  class Visitor {
   public:
    Visitor() {}

    // Called when the stream is closed.
    virtual void OnClose(QuicDataStream* stream) = 0;

   protected:
    virtual ~Visitor() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Visitor);
  };

  QuicDataStream(QuicStreamId id, QuicSession* session);

  virtual ~QuicDataStream();

  // ReliableQuicStream implementation
  virtual void OnClose() OVERRIDE;
  // By default, this is the same as priority(), however it allows streams
  // to temporarily alter effective priority.   For example if a SPDY stream has
  // compressed but not written headers it can write the headers with a higher
  // priority.
  virtual QuicPriority EffectivePriority() const OVERRIDE;
  virtual uint32 ProcessRawData(const char* data, uint32 data_len) OVERRIDE;

  // QuicSpdyDecompressor::Visitor implementation.
  virtual bool OnDecompressedData(base::StringPiece data) OVERRIDE;
  virtual void OnDecompressionError() OVERRIDE;

  virtual uint32 ProcessData(const char* data, uint32 data_len) = 0;

  // This block of functions wraps the sequencer's functions of the same
  // name.  These methods return uncompressed data until that has
  // been fully processed.  Then they simply delegate to the sequencer.
  virtual size_t Readv(const struct iovec* iov, size_t iov_len);
  virtual int GetReadableRegions(iovec* iov, size_t iov_len);
  // Returns true when all data has been read from the peer, including the fin.
  virtual bool IsDoneReading() const;
  virtual bool HasBytesToRead() const;

  // Called by the session when a decompression blocked stream
  // becomes unblocked.
  virtual void OnDecompressorAvailable();

  void set_visitor(Visitor* visitor) { visitor_ = visitor; }

  bool headers_decompressed() const { return headers_decompressed_; }

  const IPEndPoint& GetPeerAddress();

  QuicSpdyCompressor* compressor();

  // Gets the SSL connection information.
  bool GetSSLInfo(SSLInfo* ssl_info);

 protected:
  // Sets priority_ to priority.  This should only be called before bytes are
  // written to the server.
  void set_priority(QuicPriority priority);
  // This is protected because external classes should use EffectivePriority
  // instead.
  QuicPriority priority() const { return priority_; }

 private:
  friend class test::QuicDataStreamPeer;
  friend class test::ReliableQuicStreamPeer;
  friend class QuicStreamUtils;

  uint32 ProcessHeaderData();

  uint32 StripPriorityAndHeaderId(const char* data, uint32 data_len);

  bool FinishedReadingHeaders();

  Visitor* visitor_;
  // True if the headers have been completely decompresssed.
  bool headers_decompressed_;
  // The priority of the stream, once parsed.
  QuicPriority priority_;
  // ID of the header block sent by the peer, once parsed.
  QuicHeaderId headers_id_;
  // Buffer into which we write bytes from priority_ and headers_id_
  // until each is fully parsed.
  string headers_id_and_priority_buffer_;
  // Contains a copy of the decompressed headers until they are consumed
  // via ProcessData or Readv.
  string decompressed_headers_;
  // True if an error was encountered during decompression.
  bool decompression_failed_;
  // True if the priority has been read, false otherwise.
  bool priority_parsed_;

  DISALLOW_COPY_AND_ASSIGN(QuicDataStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_DATA_STREAM_H_
