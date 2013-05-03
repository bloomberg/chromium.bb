// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_spdy2.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test_spdy2 {

namespace {

// Parses a URL into the scheme, host, and path components required for a
// SPDY request.
void ParseUrl(const char* const url, std::string* scheme, std::string* host,
              std::string* path) {
  GURL gurl(url);
  path->assign(gurl.PathForRequest());
  scheme->assign(gurl.scheme());
  host->assign(gurl.host());
  if (gurl.has_port()) {
    host->append(":");
    host->append(gurl.port());
  }
}

}  // namespace

scoped_ptr<SpdyHeaderBlock> ConstructGetHeaderBlock(base::StringPiece url) {
  std::string scheme, host, path;
  ParseUrl(url.data(), &scheme, &host, &path);
  const char* const headers[] = {
    "method",  "GET",
    "url",     path.c_str(),
    "host",    host.c_str(),
    "scheme",  scheme.c_str(),
    "version", "HTTP/1.1"
  };
  scoped_ptr<SpdyHeaderBlock> header_block(new SpdyHeaderBlock());
  AppendToHeaderBlock(headers, arraysize(headers) / 2, header_block.get());
  return header_block.Pass();
}

scoped_ptr<SpdyHeaderBlock> ConstructPostHeaderBlock(base::StringPiece url,
                                                     int64 content_length) {
  std::string scheme, host, path;
  ParseUrl(url.data(), &scheme, &host, &path);
  std::string length_str = base::Int64ToString(content_length);
  const char* const headers[] = {
    "method",  "POST",
    "url",     path.c_str(),
    "host",    host.c_str(),
    "scheme",  scheme.c_str(),
    "version", "HTTP/1.1",
    "content-length", length_str.c_str()
  };
  scoped_ptr<SpdyHeaderBlock> header_block(new SpdyHeaderBlock());
  AppendToHeaderBlock(headers, arraysize(headers) / 2, header_block.get());
  return header_block.Pass();
}

SpdyFrame* ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                              scoped_ptr<SpdyHeaderBlock> headers) {
  BufferedSpdyFramer framer(kSpdyVersion2, header_info.compressed);
  SpdyFrame* frame = NULL;
  switch (header_info.kind) {
    case DATA:
      frame = framer.CreateDataFrame(header_info.id, header_info.data,
                                     header_info.data_length,
                                     header_info.data_flags);
      break;
    case SYN_STREAM:
      frame = framer.CreateSynStream(header_info.id, header_info.assoc_id,
                                     header_info.priority, 0,
                                     header_info.control_flags,
                                     header_info.compressed, headers.get());
      break;
    case SYN_REPLY:
      frame = framer.CreateSynReply(header_info.id, header_info.control_flags,
                                    header_info.compressed, headers.get());
      break;
    case RST_STREAM:
      frame = framer.CreateRstStream(header_info.id, header_info.status);
      break;
    case HEADERS:
      frame = framer.CreateHeaders(header_info.id, header_info.control_flags,
                                   header_info.compressed, headers.get());
      break;
    default:
      ADD_FAILURE();
      break;
  }
  return frame;
}

SpdyFrame* ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                              const char* const extra_headers[],
                              int extra_header_count,
                              const char* const tail[],
                              int tail_header_count) {
  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock());
  AppendToHeaderBlock(extra_headers, extra_header_count, headers.get());
  if (tail && tail_header_count)
    AppendToHeaderBlock(tail, tail_header_count, headers.get());
  return ConstructSpdyFrame(header_info, headers.Pass());
}

SpdyFrame* ConstructSpdySettings(const SettingsMap& settings) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateSettings(settings);
}

SpdyFrame* ConstructSpdyCredential(
    const SpdyCredential& credential) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateCredentialFrame(credential);
}

SpdyFrame* ConstructSpdyPing(uint32 ping_id) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreatePingFrame(ping_id);
}

SpdyFrame* ConstructSpdyGoAway() {
  return ConstructSpdyGoAway(0);
}

SpdyFrame* ConstructSpdyGoAway(SpdyStreamId last_good_stream_id) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateGoAway(last_good_stream_id, GOAWAY_OK);
}

SpdyFrame* ConstructSpdyWindowUpdate(
    const SpdyStreamId stream_id, uint32 delta_window_size) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateWindowUpdate(stream_id, delta_window_size);
}

SpdyFrame* ConstructSpdyRstStream(SpdyStreamId stream_id,
                                  SpdyRstStreamStatus status) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateRstStream(stream_id, status);
}

int ConstructSpdyHeader(const char* const extra_headers[],
                        int extra_header_count,
                        char* buffer,
                        int buffer_length,
                        int index) {
  const char* this_header = NULL;
  const char* this_value = NULL;
  if (!buffer || !buffer_length)
    return 0;
  *buffer = '\0';
  // Sanity check: Non-empty header list.
  DCHECK(NULL != extra_headers) << "NULL extra headers pointer";
  // Sanity check: Index out of range.
  DCHECK((index >= 0) && (index < extra_header_count))
      << "Index " << index
      << " out of range [0, " << extra_header_count << ")";
  this_header = extra_headers[index * 2];
  // Sanity check: Non-empty header.
  if (!*this_header)
    return 0;
  std::string::size_type header_len = strlen(this_header);
  if (!header_len)
    return 0;
  this_value = extra_headers[1 + (index * 2)];
  // Sanity check: Non-empty value.
  if (!*this_value)
    this_value = "";
  int n = base::snprintf(buffer,
                         buffer_length,
                         "%s: %s\r\n",
                         this_header,
                         this_value);
  return n;
}

SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     int stream_id,
                                     RequestPriority request_priority,
                                     SpdyFrameType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize) {
  EXPECT_GE(type, FIRST_CONTROL_TYPE);
  EXPECT_LE(type, LAST_CONTROL_TYPE);
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   compressed,
                                   stream_id,
                                   request_priority,
                                   type,
                                   flags,
                                   kHeaders,
                                   kHeadersSize,
                                   0);
}

SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     SpdyStreamId stream_id,
                                     RequestPriority request_priority,
                                     SpdyFrameType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize,
                                     SpdyStreamId associated_stream_id) {
  EXPECT_GE(type, FIRST_CONTROL_TYPE);
  EXPECT_LE(type, LAST_CONTROL_TYPE);
  const SpdyHeaderInfo kSynStartHeader = {
    type,                         // Kind = Syn
    stream_id,                    // Stream ID
    associated_stream_id,         // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(request_priority, 2),
                                  // Priority
    kSpdyCredentialSlotUnused,
    flags,                        // Control Flags
    compressed,                   // Compressed
    RST_STREAM_INVALID,           // Status
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE                // Data Flags
  };
  return ConstructSpdyFrame(kSynStartHeader,
                            extra_headers,
                            extra_header_count,
                            kHeaders,
                            kHeadersSize / 2);
}

SpdyFrame* ConstructSpdyGet(const char* const url,
                            bool compressed,
                            SpdyStreamId stream_id,
                            RequestPriority request_priority) {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,             // Kind = Syn
    stream_id,              // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(request_priority, 2),
                            // Priority
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_FIN,       // Control Flags
    compressed,             // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  return ConstructSpdyFrame(kSynStartHeader, ConstructGetHeaderBlock(url));
}

SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority) {
  return ConstructSpdyGet(extra_headers, extra_header_count, compressed,
                          stream_id, request_priority, true);
}

SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority,
                            bool direct) {
  const char* const kStandardGetHeaders[] = {
    "method", "GET",
    "url", (direct ? "/" : "http://www.google.com/"),
    "host", "www.google.com",
    "scheme", "http",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   compressed,
                                   stream_id,
                                   request_priority,
                                   SYN_STREAM,
                                   CONTROL_FLAG_FIN,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

SpdyFrame* ConstructSpdyConnect(const char* const extra_headers[],
                                int extra_header_count,
                                int stream_id) {
  const char* const kConnectHeaders[] = {
    "method", "CONNECT",
    "url", "www.google.com:443",
    "host", "www.google.com",
    "version", "HTTP/1.1",
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   /*compressed*/ false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kConnectHeaders,
                                   arraysize(kConnectHeaders));
}

SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status", "200",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   associated_stream_id);
}

SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status", "200 OK",
    "url", url,
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   associated_stream_id);

}
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url,
                             const char* status,
                             const char* location) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status",  status,
    "location", location,
    "url", url,
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   associated_stream_id);
}

SpdyFrame* ConstructSpdyPush(int stream_id,
                             int associated_stream_id,
                             const char* url) {
  const char* const kStandardGetHeaders[] = {
    "url", url
  };
  return ConstructSpdyControlFrame(0,
                                   0,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders),
                                   associated_stream_id);
}

SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                    const char* const extra_headers[],
                                    int extra_header_count) {
  const char* const kStandardGetHeaders[] = {
    "status", "200 OK",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   HEADERS,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

SpdyFrame* ConstructSpdySynReplyError(const char* const status,
                                      const char* const* const extra_headers,
                                      int extra_header_count,
                                      int stream_id) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status", status,
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id) {
  static const char* const kExtraHeaders[] = {
    "location", "http://www.foo.com/index.php",
  };
  return ConstructSpdySynReplyError("301 Moved Permanently", kExtraHeaders,
                                    arraysize(kExtraHeaders)/2, stream_id);
}

SpdyFrame* ConstructSpdySynReplyError(int stream_id) {
  return ConstructSpdySynReplyError("500 Internal Server Error", NULL, 0, 1);
}

SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                    int extra_header_count,
                                    int stream_id) {
  static const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status", "200",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

SpdyFrame* ConstructSpdyPost(const char* url,
                             int64 content_length,
                             const char* const extra_headers[],
                             int extra_header_count) {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,             // Kind = Syn
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(LOWEST, 2),
                            // Priority
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,      // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  return ConstructSpdyFrame(
      kSynStartHeader, ConstructPostHeaderBlock(url, content_length));
}

SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                    int extra_header_count) {
  const char* post_headers[] = {
    "method", "POST",
    "url", "/",
    "host", "www.google.com",
    "scheme", "http",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   post_headers,
                                   arraysize(post_headers));
}

SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                     int extra_header_count) {
  static const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    "status", "200",
    "url", "/index.php",
    "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

SpdyFrame* ConstructSpdyBodyFrame(int stream_id, bool fin) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateDataFrame(
      stream_id, kUploadData, kUploadDataSize,
      fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                  uint32 len, bool fin) {
  BufferedSpdyFramer framer(2, false);
  return framer.CreateDataFrame(
      stream_id, data, len, fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

SpdyFrame* ConstructWrappedSpdyFrame(const scoped_ptr<SpdyFrame>& frame,
                                     int stream_id) {
  return ConstructSpdyBodyFrame(stream_id, frame->data(),
                                frame->size(), false);
}

int ConstructSpdyReplyString(const char* const extra_headers[],
                             int extra_header_count,
                             char* buffer,
                             int buffer_length) {
  int frame_size = 0;
  char* buffer_write = buffer;
  int buffer_left = buffer_length;
  SpdyHeaderBlock headers;
  if (!buffer || !buffer_length)
    return 0;
  // Copy in the extra headers.
  AppendToHeaderBlock(extra_headers, extra_header_count, &headers);
  // The iterator gets us the list of header/value pairs in sorted order.
  SpdyHeaderBlock::iterator next = headers.begin();
  SpdyHeaderBlock::iterator last = headers.end();
  for ( ; next != last; ++next) {
    // Write the header.
    int value_len, current_len, offset;
    const char* header_string = next->first.c_str();
    frame_size += AppendToBuffer(header_string,
                                 next->first.length(),
                                 &buffer_write,
                                 &buffer_left);
    frame_size += AppendToBuffer(": ",
                                 strlen(": "),
                                 &buffer_write,
                                 &buffer_left);
    // Write the value(s).
    const char* value_string = next->second.c_str();
    // Check if it's split among two or more values.
    value_len = next->second.length();
    current_len = strlen(value_string);
    offset = 0;
    // Handle the first N-1 values.
    while (current_len < value_len) {
      // Finish this line -- write the current value.
      frame_size += AppendToBuffer(value_string + offset,
                                   current_len - offset,
                                   &buffer_write,
                                   &buffer_left);
      frame_size += AppendToBuffer("\n",
                                   strlen("\n"),
                                   &buffer_write,
                                   &buffer_left);
      // Advance to next value.
      offset = current_len + 1;
      current_len += 1 + strlen(value_string + offset);
      // Start another line -- add the header again.
      frame_size += AppendToBuffer(header_string,
                                   next->first.length(),
                                   &buffer_write,
                                   &buffer_left);
      frame_size += AppendToBuffer(": ",
                                   strlen(": "),
                                   &buffer_write,
                                   &buffer_left);
    }
    EXPECT_EQ(value_len, current_len);
    // Copy the last (or only) value.
    frame_size += AppendToBuffer(value_string + offset,
                                 value_len - offset,
                                 &buffer_write,
                                 &buffer_left);
    frame_size += AppendToBuffer("\n",
                                 strlen("\n"),
                                 &buffer_write,
                                 &buffer_left);
  }
  return frame_size;
}

const SpdyHeaderInfo MakeSpdyHeader(SpdyFrameType type) {
  const SpdyHeaderInfo kHeader = {
    type,                         // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(LOWEST, 2),  // Priority
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_FIN,       // Control Flags
    false,                        // Compressed
    RST_STREAM_INVALID,           // Status
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE          // Data Flags
  };
  return kHeader;
}

}  // namespace test_spdy2
}  // namespace net
