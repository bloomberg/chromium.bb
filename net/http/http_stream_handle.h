// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef NET_HTTP_HTTP_STREAM_HANDLE_H_
#define NET_HTTP_HTTP_STREAM_HANDLE_H_
#pragma once

#include <string>

#include "net/http/http_stream.h"

#include "net/socket/client_socket_handle.h"

namespace net {

struct HttpRequestInfo;

// The HttpStreamHandle provides a container for a ClientSocketHandle and
// a stream.  The HttpStream does not own the ClientSocketHandle which it uses.
// The HttpStreamHandle container can be used in cases where you just want a
// handle to the stream but you don't want to manage the lifecycle of the
// underlying ClientSocketHandle manually.
class HttpStreamHandle : public HttpStream {
 public:
  HttpStreamHandle(ClientSocketHandle* connection, HttpStream* stream)
      : connection_(connection), stream_(stream) {
    DCHECK(stream_.get());
  }
  virtual ~HttpStreamHandle() {}

  HttpStream* stream() { return stream_.get(); }

  // HttpStream interface
  int InitializeStream(const HttpRequestInfo* request_info,
                       const BoundNetLog& net_log,
                       CompletionCallback* callback) {
    return stream_->InitializeStream(request_info, net_log, callback);
  }

  int SendRequest(const std::string& request_headers,
                  UploadDataStream* request_body,
                  HttpResponseInfo* response,
                  CompletionCallback* callback) {
    return stream_->SendRequest(request_headers, request_body, response,
                                callback);
  }

  uint64 GetUploadProgress() const {
    return stream_->GetUploadProgress();
  }

  int ReadResponseHeaders(CompletionCallback* callback) {
    return stream_->ReadResponseHeaders(callback);
  }

  const HttpResponseInfo* GetResponseInfo() const {
    return stream_->GetResponseInfo();
  }

  int ReadResponseBody(IOBuffer* buf, int buf_len,
                       CompletionCallback* callback) {
    return stream_->ReadResponseBody(buf, buf_len, callback);
  }

  void Close(bool not_reusable) {
    stream_->Close(not_reusable);
  }

  bool IsResponseBodyComplete() const {
    return stream_->IsResponseBodyComplete();
  }

  bool CanFindEndOfResponse() const {
    return stream_->CanFindEndOfResponse();
  }

  bool IsMoreDataBuffered() const {
    return stream_->IsMoreDataBuffered();
  }

  bool IsConnectionReused() const {
    return stream_->IsConnectionReused();
  }

  void SetConnectionReused() {
    stream_->SetConnectionReused();
  }

  void GetSSLInfo(SSLInfo* ssl_info) {
    stream_->GetSSLInfo(ssl_info);
  }

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) {
    stream_->GetSSLCertRequestInfo(cert_request_info);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest, WindowUpdateSent);

  scoped_ptr<ClientSocketHandle> connection_;
  scoped_ptr<HttpStream> stream_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamHandle);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_HANDLE_H_

