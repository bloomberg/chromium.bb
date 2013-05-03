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

// Constructs a HeaderBlock for a GET request for the given URL.
scoped_ptr<SpdyHeaderBlock> ConstructGetHeaderBlock(base::StringPiece url);

// Constructs a HeaderBlock for a POST request for the given URL.
scoped_ptr<SpdyHeaderBlock> ConstructPostHeaderBlock(base::StringPiece url,
                                                     int64 content_length);

// Construct a SPDY frame.
SpdyFrame* ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                              scoped_ptr<SpdyHeaderBlock> headers);

// Construct a SPDY frame.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |tail| is any (relatively constant) header-value pairs to add.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                              const char* const extra_headers[],
                              int extra_header_count,
                              const char* const tail[],
                              int tail_header_count);

// Construct a generic SpdyControlFrame.
SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     int stream_id,
                                     RequestPriority request_priority,
                                     SpdyFrameType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize);
SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     SpdyStreamId stream_id,
                                     RequestPriority request_priority,
                                     SpdyFrameType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize,
                                     SpdyStreamId associated_stream_id);

// Construct an expected SPDY reply string.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |buffer| is the buffer we're filling in.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyReplyString(const char* const extra_headers[],
                             int extra_header_count,
                             char* buffer,
                             int buffer_length);

// Construct an expected SPDY SETTINGS frame.
// |settings| are the settings to set.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdySettings(const SettingsMap& settings);

// Construct an expected SPDY CREDENTIAL frame.
// |credential| is the credential to send.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyCredential(const SpdyCredential& credential);

// Construct a SPDY PING frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyPing(uint32 ping_id);

// Construct a SPDY GOAWAY frame with last_good_stream_id = 0.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyGoAway();

// Construct a SPDY GOAWAY frame with the specified last_good_stream_id.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyGoAway(SpdyStreamId last_good_stream_id);

// Construct a SPDY WINDOW_UPDATE frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyWindowUpdate(SpdyStreamId, uint32 delta_window_size);

// Construct a SPDY RST_STREAM frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyRstStream(SpdyStreamId stream_id,
                                  SpdyRstStreamStatus status);

// Construct a single SPDY header entry, for validation.
// |extra_headers| are the extra header-value pairs.
// |buffer| is the buffer we're filling in.
// |index| is the index of the header we want.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyHeader(const char* const extra_headers[],
                        int extra_header_count,
                        char* buffer,
                        int buffer_length,
                        int index);

// Constructs a standard SPDY GET SYN frame, optionally compressed
// for the url |url|.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const url,
                            bool compressed,
                            SpdyStreamId stream_id,
                            RequestPriority request_priority);

// Constructs a standard SPDY GET SYN frame, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority);

// Constructs a standard SPDY GET SYN frame, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.  If |direct| is false, the
// the full url will be used instead of simply the path.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority,
                            bool direct);

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
SpdyFrame* ConstructSpdyConnect(const char* const extra_headers[],
                                int extra_header_count,
                                int stream_id);

// Constructs a standard SPDY push SYN frame.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id);
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
SpdyFrame* ConstructSpdyPush(int stream_id,
                             int associated_stream_id,
                             const char* url);

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
                             int64 content_length,
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
