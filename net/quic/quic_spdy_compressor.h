// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SPDY_COMPRESSOR_H_
#define NET_QUIC_QUIC_SPDY_COMPRESSOR_H_

#include <string>

#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/spdy/spdy_framer.h"

namespace net {

// Handles the compression of request/response headers blocks.  The
// serialized format is:
// uint32 - Header ID
// uint32 - Compressed header length
// ... - Compressed data
//
class NET_EXPORT_PRIVATE QuicSpdyCompressor {
 public:
  QuicSpdyCompressor();
  ~QuicSpdyCompressor();

  std::string CompressHeaders(const SpdyHeaderBlock& headers);

 private:
  SpdyFramer spdy_framer_;
  QuicHeaderId header_sequence_id_;

  DISALLOW_COPY_AND_ASSIGN(QuicSpdyCompressor);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SPDY_COMPRESSOR_H_
