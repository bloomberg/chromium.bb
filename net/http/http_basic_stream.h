// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpBasicStream is a simple implementation of HttpStream.  It assumes it is
// not sharing a sharing with any other HttpStreams, therefore it just reads and
// writes directly to the Http Stream.

#ifndef NET_HTTP_HTTP_BASIC_STREAM_H_
#define NET_HTTP_HTTP_BASIC_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "net/http/http_basic_state.h"
#include "net/http/http_stream.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HttpStreamParser;
class IOBuffer;

class HttpBasicStream : public HttpStream {
 public:
  // Constructs a new HttpBasicStream. InitializeStream must be called to
  // initialize it correctly.
  HttpBasicStream(ClientSocketHandle* connection, bool using_proxy);
  virtual ~HttpBasicStream();

  // HttpStream methods:
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) override;

  virtual int SendRequest(const HttpRequestHeaders& headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) override;

  virtual UploadProgress GetUploadProgress() const override;

  virtual int ReadResponseHeaders(const CompletionCallback& callback) override;

  virtual int ReadResponseBody(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) override;

  virtual void Close(bool not_reusable) override;

  virtual HttpStream* RenewStreamForAuth() override;

  virtual bool IsResponseBodyComplete() const override;

  virtual bool CanFindEndOfResponse() const override;

  virtual bool IsConnectionReused() const override;

  virtual void SetConnectionReused() override;

  virtual bool IsConnectionReusable() const override;

  virtual int64 GetTotalReceivedBytes() const override;

  virtual bool GetLoadTimingInfo(
      LoadTimingInfo* load_timing_info) const override;

  virtual void GetSSLInfo(SSLInfo* ssl_info) override;

  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) override;

  virtual bool IsSpdyHttpStream() const override;

  virtual void Drain(HttpNetworkSession* session) override;

  virtual void SetPriority(RequestPriority priority) override;

 private:
  HttpStreamParser* parser() const { return state_.parser(); }

  HttpBasicState state_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
