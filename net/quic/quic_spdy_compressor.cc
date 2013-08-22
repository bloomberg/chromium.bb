// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_spdy_compressor.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

using std::string;

namespace net {

QuicSpdyCompressor::QuicSpdyCompressor()
    : spdy_framer_(SPDY3),
      header_sequence_id_(1) {
  spdy_framer_.set_enable_compression(true);
}

QuicSpdyCompressor::~QuicSpdyCompressor() {
}

string QuicSpdyCompressor::CompressHeaders(
    const SpdyHeaderBlock& headers) {
  // TODO(rch): Modify the SpdyFramer to expose a
  // CreateCompressedHeaderBlock method, or some such.
  SpdyStreamId stream_id = 3;    // unused.
  scoped_ptr<SpdyFrame> frame(spdy_framer_.CreateHeaders(
      stream_id, CONTROL_FLAG_NONE, true, &headers));

  // The size of the spdy HEADER frame's fixed prefix which
  // needs to be stripped off from the resulting frame.
  const size_t header_frame_prefix_len = 12;
  string serialized = string(frame->data() + header_frame_prefix_len,
                             frame->size() - header_frame_prefix_len);
  uint32 serialized_len = serialized.length();
  char id_str[sizeof(header_sequence_id_)];
  memcpy(&id_str, &header_sequence_id_, sizeof(header_sequence_id_));
  char len_str[sizeof(serialized_len)];
  memcpy(&len_str, &serialized_len, sizeof(serialized_len));
  string compressed;
  compressed.reserve(arraysize(id_str) + arraysize(len_str) + serialized_len);
  compressed.append(id_str, arraysize(id_str));
  compressed.append(len_str, arraysize(len_str));
  compressed.append(serialized);
  ++header_sequence_id_;
  return compressed;
}

}  // namespace net
