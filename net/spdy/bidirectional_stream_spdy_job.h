// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_JOB_H_
#define NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_JOB_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/bidirectional_stream_job.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_request_info.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"

namespace base {
class Timer;
}  // namespace base

namespace net {

class BoundNetLog;
class IOBuffer;
class SpdyHeaderBlock;

class NET_EXPORT_PRIVATE BidirectionalStreamSpdyJob
    : public BidirectionalStreamJob,
      public SpdyStream::Delegate {
 public:
  explicit BidirectionalStreamSpdyJob(
      const base::WeakPtr<SpdySession>& spdy_session);

  ~BidirectionalStreamSpdyJob() override;

  // BidirectionalStreamJob implementation:
  void Start(const BidirectionalStreamRequestInfo* request_info,
             const BoundNetLog& net_log,
             BidirectionalStreamJob::Delegate* delegate,
             scoped_ptr<base::Timer> timer) override;
  int ReadData(IOBuffer* buf, int buf_len) override;
  void SendData(IOBuffer* data, int length, bool end_stream) override;
  void Cancel() override;
  NextProto GetProtocol() const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;

  // SpdyStream::Delegate implementation:
  void OnRequestHeadersSent() override;
  SpdyResponseHeadersStatus OnResponseHeadersUpdated(
      const SpdyHeaderBlock& response_headers) override;
  void OnDataReceived(scoped_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const SpdyHeaderBlock& trailers) override;
  void OnClose(int status) override;

 private:
  void SendRequestHeaders();
  void OnStreamInitialized(int rv);
  void ScheduleBufferedRead();
  void DoBufferedRead();
  bool ShouldWaitForMoreBufferedData() const;

  const base::WeakPtr<SpdySession> spdy_session_;
  const BidirectionalStreamRequestInfo* request_info_;
  BidirectionalStreamJob::Delegate* delegate_;
  scoped_ptr<base::Timer> timer_;
  SpdyStreamRequest stream_request_;
  base::WeakPtr<SpdyStream> stream_;

  NextProto negotiated_protocol_;

  // Buffers the data as it arrives asynchronously from the stream.
  SpdyReadQueue read_data_queue_;
  // Whether received more data has arrived since started waiting.
  bool more_read_data_pending_;
  // User provided read buffer for ReadData() response.
  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_len_;

  // Whether OnClose has been invoked.
  bool stream_closed_;
  // Status reported in OnClose.
  int closed_stream_status_;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes received over the network for |stream_| while it was open.
  int64_t closed_stream_received_bytes_;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes sent over the network for |stream_| while it was open.
  int64_t closed_stream_sent_bytes_;

  base::WeakPtrFactory<BidirectionalStreamSpdyJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BidirectionalStreamSpdyJob);
};

}  // namespace net

#endif  // NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_JOB_H_
