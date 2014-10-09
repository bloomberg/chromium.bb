// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_
#define NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_stream.h"

namespace net {

// A dummy HttpStream with no body used when a redirect is returned
// from a proxy.
class ProxyConnectRedirectHttpStream : public HttpStream {
 public:
  // |load_timing_info| is the info that should be returned by
  // GetLoadTimingInfo(), or NULL if there is none. Does not take
  // ownership of |load_timing_info|.
  explicit ProxyConnectRedirectHttpStream(LoadTimingInfo* load_timing_info);
  virtual ~ProxyConnectRedirectHttpStream();

  // All functions below are expected not to be called except for the
  // marked one.

  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) override;
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) override;
  virtual int ReadResponseHeaders(const CompletionCallback& callback) override;
  virtual int ReadResponseBody(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) override;

  // This function may be called.
  virtual void Close(bool not_reusable) override;

  virtual bool IsResponseBodyComplete() const override;

  // This function may be called.
  virtual bool CanFindEndOfResponse() const override;

  virtual bool IsConnectionReused() const override;
  virtual void SetConnectionReused() override;
  virtual bool IsConnectionReusable() const override;

  virtual int64 GetTotalReceivedBytes() const override;

  // This function may be called.
  virtual bool GetLoadTimingInfo(
      LoadTimingInfo* load_timing_info) const override;

  virtual void GetSSLInfo(SSLInfo* ssl_info) override;
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) override;
  virtual bool IsSpdyHttpStream() const override;
  virtual void Drain(HttpNetworkSession* session) override;

  // This function may be called.
  virtual void SetPriority(RequestPriority priority) override;

  virtual UploadProgress GetUploadProgress() const override;
  virtual HttpStream* RenewStreamForAuth() override;

 private:
  bool has_load_timing_info_;
  LoadTimingInfo load_timing_info_;
};

}  // namespace net

#endif  // NET_HTTP_PROXY_CONNECT_REDIRECT_HTTP_STREAM_H_
