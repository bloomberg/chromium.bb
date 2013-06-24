// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_SPDY2_H_
#define NET_SPDY_SPDY_TEST_UTIL_SPDY2_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_transaction_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"

namespace net {

namespace test_spdy2 {

// Can't find a function you're looking for?  ttuttle is migrating functions
// from here into methods in the SpdyTestUtil class in spdy_test_common.h.

// Constructs a standard SPDY push SYN frame.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url);
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url,
                             const char* status,
                             const char* location);

SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                    const char* const extra_headers[],
                                    int extra_header_count);

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                    int extra_header_count,
                                    int stream_id);

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id);

// Constructs a standard SPDY SYN_REPLY frame with an Internal Server
// Error status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(int stream_id);

// Constructs a standard SPDY SYN_REPLY frame with the specified status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(const char* const status,
                                      const char* const* const extra_headers,
                                      int extra_header_count,
                                      int stream_id);

// Constructs a standard SPDY POST SYN frame.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPost(const char* url,
                             SpdyStreamId stream_id,
                             int64 content_length,
                             RequestPriority priority,
                             const char* const extra_headers[],
                             int extra_header_count);

// Constructs a chunked transfer SPDY POST SYN frame.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                    int extra_header_count);

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY POST.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                     int extra_header_count);

// Constructs a single SPDY data frame with the contents "hello!"
SpdyFrame* ConstructSpdyBodyFrame(int stream_id,
                                  bool fin);

// Constructs a single SPDY data frame with the given content.
SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                  uint32 len, bool fin);

// Wraps |frame| in the payload of a data frame in stream |stream_id|.
SpdyFrame* ConstructWrappedSpdyFrame(const scoped_ptr<SpdyFrame>& frame,
                                     int stream_id);

const SpdyHeaderInfo MakeSpdyHeader(SpdyFrameType type);

}  // namespace test_spdy2

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_SPDY2_H_
