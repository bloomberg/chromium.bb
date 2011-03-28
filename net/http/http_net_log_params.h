// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NET_LOG_PARAMS_H_
#define NET_HTTP_HTTP_NET_LOG_PARAMS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_log.h"
#include "net/http/http_request_headers.h"

class Value;

namespace net {

class HttpResponseHeaders;

class NetLogHttpRequestParameter : public NetLog::EventParameters {
 public:
  NetLogHttpRequestParameter(const std::string& line,
                             const HttpRequestHeaders& headers);

  const HttpRequestHeaders& GetHeaders() const {
    return headers_;
  }

  const std::string& GetLine() const {
    return line_;
  }

  // NetLog::EventParameters
  virtual Value* ToValue() const;

 private:
  virtual ~NetLogHttpRequestParameter();

  const std::string line_;
  HttpRequestHeaders headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogHttpRequestParameter);
};

class NetLogHttpResponseParameter : public NetLog::EventParameters {
 public:
  explicit NetLogHttpResponseParameter(
      const scoped_refptr<HttpResponseHeaders>& headers);

  const HttpResponseHeaders& GetHeaders() const {
    return *headers_;
  }

  // NetLog::EventParameters
  virtual Value* ToValue() const;

 private:
  virtual ~NetLogHttpResponseParameter();

  const scoped_refptr<HttpResponseHeaders> headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogHttpResponseParameter);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NET_LOG_PARAMS_H_

