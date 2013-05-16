// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SPDY_DECOMPRESSOR_H_
#define NET_QUIC_QUIC_SPDY_DECOMPRESSOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/spdy/spdy_framer.h"

namespace net {

class SpdyFramerVisitor;

// Handles the compression of request/response headers blocks.
class NET_EXPORT_PRIVATE QuicSpdyDecompressor {
 public:
  // Interface that receives callbacks with decompressed data as it
  // becomes available.
  class NET_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}
    virtual bool OnDecompressedData(base::StringPiece data) = 0;
    virtual void OnDecompressionError() = 0;
  };

  QuicSpdyDecompressor();
  ~QuicSpdyDecompressor();

  // Decompresses the data in |data| and invokes |OnDecompressedData|,
  // possibly multiple times, on |visitor|.  Returns number of bytes
  // consumed from |data|.
  size_t DecompressData(base::StringPiece data, Visitor* visitor);

  QuicHeaderId current_header_id() { return current_header_id_; }

 private:
  void ResetForNextHeaders();

  SpdyFramer spdy_framer_;
  scoped_ptr<SpdyFramerVisitor> spdy_visitor_;
  // ID of the header currently being parsed.
  QuicHeaderId current_header_id_;
  // True when the size of the headers has been parsed.
  bool has_current_compressed_size_;
  // Size of the headers being parsed.
  uint32 current_compressed_size_;
  // Buffer into which the partial compressed size is written until
  // it is fully parsed.
  std::string compressed_size_buffer_;
  // Number of compressed bytes consumed, out of the total in
  // |current_compressed_size_|.
  uint32 compressed_bytes_consumed_;
  DISALLOW_COPY_AND_ASSIGN(QuicSpdyDecompressor);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SPDY_DECOMPRESSOR_H_
