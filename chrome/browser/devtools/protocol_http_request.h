// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_HTTP_REQUEST_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_HTTP_REQUEST_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"

namespace net {
class URLRequestContextGetter;
}

class Profile;

class ProtocolHttpRequest : public net::URLRequest::Delegate {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;

  ProtocolHttpRequest(Profile* profile,
                      const std::string& url,
                      const Callback& callback);

 private:
  virtual ~ProtocolHttpRequest();

  void Start();

  // net::URLRequest::Delegate implementation.
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  void RespondOnUIThread();

  net::URLRequestContextGetter* request_context_;
  GURL url_;
  Callback callback_;

  int result_;
  std::string data_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolHttpRequest);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_HTTP_REQUEST_H_
