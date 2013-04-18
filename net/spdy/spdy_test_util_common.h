// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
#define NET_SPDY_SPDY_TEST_UTIL_COMMON_H_

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_protocol.h"

class GURL;

namespace net {

class BoundNetLog;
class SpdySession;
class SpdyStream;
class SpdyStreamRequest;

// Default upload data used by both, mock objects and framer when creating
// data frames.
const char kDefaultURL[] = "http://www.google.com";
const char kUploadData[] = "hello!";
const int kUploadDataSize = arraysize(kUploadData)-1;

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks);

// Chop a frame into an array of MockReads.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockReads.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const SpdyFrame& frame, int num_chunks);

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendToHeaderBlock(const char* const extra_headers[],
                         int extra_header_count,
                         SpdyHeaderBlock* headers);

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

// Create an async MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req);

// Create an async MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq);

MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode);

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp);

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq);

MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode);

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames, int num_frames,
                  char* buff, int buff_len);

// Returns the SpdyPriority embedded in the given frame.  Returns true
// and fills in |priority| on success.
bool GetSpdyPriority(int version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority);

// Tries to create a stream in |session| synchronously. Returns NULL
// on failure.
scoped_refptr<SpdyStream> CreateStreamSynchronously(
    const scoped_refptr<SpdySession>& session,
    const GURL& url,
    RequestPriority priority,
    const BoundNetLog& net_log);

// Helper class used by some tests to release two streams as soon as
// one is created.
class StreamReleaserCallback : public TestCompletionCallbackBase {
 public:
  StreamReleaserCallback(SpdySession* session,
                         SpdyStream* first_stream);

  virtual ~StreamReleaserCallback();

  // Returns a callback that releases |request|'s stream as well as
  // |first_stream|.
  CompletionCallback MakeCallback(SpdyStreamRequest* request);

 private:
  void OnComplete(SpdyStreamRequest* request, int result);

  scoped_refptr<SpdySession> session_;
  scoped_refptr<SpdyStream> first_stream_;
};

const size_t kSpdyCredentialSlotUnused = 0;

// This struct holds information used to construct spdy control and data frames.
struct SpdyHeaderInfo {
  SpdyFrameType kind;
  SpdyStreamId id;
  SpdyStreamId assoc_id;
  SpdyPriority priority;
  size_t credential_slot;  // SPDY3 only
  SpdyControlFlags control_flags;
  bool compressed;
  SpdyRstStreamStatus status;
  const char* data;
  uint32 data_length;
  SpdyDataFlags data_flags;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
