// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/spdy_utils.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

using std::string;

namespace net {

// static
SpdyHeaderBlock SpdyUtils::ConvertSpdy3ResponseHeadersToSpdy4(
    SpdyHeaderBlock response_headers) {
  // SPDY/4 headers include neither the version field nor the response details.
  response_headers.erase(":version");
  StringPiece status_value = response_headers[":status"];
  size_t end_of_code = status_value.find(' ');
  if (end_of_code != string::npos) {
    response_headers[":status"] = status_value.substr(0, end_of_code);
  }
  return response_headers;
}

// static
string SpdyUtils::SerializeUncompressedHeaders(const SpdyHeaderBlock& headers) {
  SpdyMajorVersion spdy_version = HTTP2;

  size_t length = SpdyFramer::GetSerializedLength(spdy_version, &headers);
  SpdyFrameBuilder builder(length, spdy_version);
  SpdyFramer::WriteHeaderBlock(&builder, spdy_version, &headers);
  scoped_ptr<SpdyFrame> block(builder.take());
  return string(block->data(), length);
}

}  // namespace net
