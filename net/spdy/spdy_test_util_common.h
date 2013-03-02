// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
#define NET_SPDY_SPDY_TEST_UTIL_COMMON_H_

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/spdy/spdy_protocol.h"

class GURL;

namespace net {

class BoundNetLog;
class SpdySession;
class SpdyStream;
class SpdyStreamRequest;

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

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
