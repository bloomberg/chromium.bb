// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_H_
#define NET_SPDY_SPDY_TEST_UTIL_H_

#include "base/basictypes.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_framer.h"

namespace net {

const uint8 kGetBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

const uint8 kGetSynCompressed[] = {
  0x80, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x47,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0xc0, 0x00, 0x38, 0xea, 0xdf, 0xa2, 0x51, 0xb2,
  0x62, 0x60, 0x66, 0x60, 0xcb, 0x05, 0xe6, 0xc3,
  0xfc, 0x14, 0x06, 0x66, 0x77, 0xd7, 0x10, 0x06,
  0x66, 0x90, 0xa0, 0x58, 0x46, 0x49, 0x49, 0x81,
  0x95, 0xbe, 0x3e, 0x30, 0xe2, 0xf5, 0xd2, 0xf3,
  0xf3, 0xd3, 0x73, 0x52, 0xf5, 0x92, 0xf3, 0x73,
  0xf5, 0x19, 0xd8, 0xa1, 0x1a, 0x19, 0x38, 0x60,
  0xe6, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

const uint8 kPostSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                      // header
  0x00, 0x00, 0x00, 0x4a,                                      // flags, len
  0x00, 0x00, 0x00, 0x01,                                      // stream id
  0x00, 0x00, 0x00, 0x00,                                      // associated
  0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x04, 'P', 'O', 'S', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

const uint8 kPostUploadFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x0c,                                        // FIN flag
  'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0'
};

// The response
const uint8 kPostSynReply[] = {
  0x80, 0x01, 0x00, 0x02,                                        // header
  0x00, 0x00, 0x00, 0x45,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x04,                                        // 4 headers
  0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
  0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
  0x00, 0x03, '2', '0', '0',                                     // "200"
  0x00, 0x03, 'u', 'r', 'l',                                     // "url"
  // "/index.php"
  0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
};

const uint8 kPostBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

const uint8 kGoAway[] = {
  0x80, 0x01, 0x00, 0x07,  // header
  0x00, 0x00, 0x00, 0x04,  // flags, len
  0x00, 0x00, 0x00, 0x00,  // last-accepted-stream-id
};

// ----------------------------------------------------------------------------

// NOTE: In GCC, on a Mac, this can't be in an anonymous namespace!
// This struct holds information used to construct spdy control and data frames.
struct SpdyHeaderInfo {
  spdy::SpdyControlType kind;
  spdy::SpdyStreamId id;
  spdy::SpdyStreamId assoc_id;
  spdy::SpdyPriority priority;
  spdy::SpdyControlFlags control_flags;
  bool compressed;
  spdy::SpdyStatusCodes status;
  const char* data;
  uint32 data_length;
  spdy::SpdyDataFlags data_flags;
};

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const spdy::SpdyFrame* frame, int num_chunks);

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendHeadersToSpdyFrame(const char* const extra_headers[],
                              int extra_header_count,
                              spdy::SpdyHeaderBlock* headers);

// Writes |str| of the given |len| to the buffer pointed to by |buffer_handle|.
// Uses a template so buffer_handle can be a char* or an unsigned char*.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written into *|buffer_handle|
template<class T>
int AppendToBuffer(const char* str,
                   int len,
                   T** buffer_handle,
                   int* buffer_len_remaining) {
  DCHECK_GT(len, 0);
  DCHECK(NULL != buffer_handle) << "NULL buffer handle";
  DCHECK(NULL != *buffer_handle) << "NULL pointer";
  DCHECK(NULL != buffer_len_remaining)
      << "NULL buffer remainder length pointer";
  DCHECK_GE(*buffer_len_remaining, len) << "Insufficient buffer size";
  memcpy(*buffer_handle, str, len);
  *buffer_handle += len;
  *buffer_len_remaining -= len;
  return len;
}

// Writes |val| to a location of size |len|, in big-endian format.
// in the buffer pointed to by |buffer_handle|.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written
int AppendToBuffer(int val,
                   int len,
                   unsigned char** buffer_handle,
                   int* buffer_len_remaining);

// Construct a SPDY packet.
// |head| is the start of the packet, up to but not including
// the header value pairs.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |tail| is any (relatively constant) header-value pairs to add.
// |buffer| is the buffer we're filling in.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyPacket(const SpdyHeaderInfo* header_info,
                                     const char* const extra_headers[],
                                     int extra_header_count,
                                     const char* const tail[],
                                     int tail_header_count);

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
spdy::SpdyFrame* ConstructSpdySettings(spdy::SpdySettings settings);

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

// Constructs a standard SPDY GET packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count);

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
spdy::SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                          int extra_header_count);

// Create an async MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(spdy::SpdyFrame* req);

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(spdy::SpdyFrame* resp);

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(spdy::SpdyFrame* resp, int seq);

}  // namespace net

#endif // NET_SPDY_SPDY_TEST_UTIL_H_
