// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NET_LOG_PARAMS_H_
#define NET_HTTP_HTTP_NET_LOG_PARAMS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace net {

class NetLogHttpRequestParameter : public NetLog::EventParameters {
 public:
  NetLogHttpRequestParameter(const std::string& line,
                             const HttpRequestHeaders& headers)
      : line_(line) {
    headers_.CopyFrom(headers);
  }

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(L"line", line_);
    ListValue* headers = new ListValue();
    HttpRequestHeaders::Iterator iterator(headers_);
    while (iterator.GetNext()) {
      headers->Append(
          new StringValue(StringPrintf("%s: %s",
                                       iterator.name().c_str(),
                                       iterator.value().c_str())));
    }
    dict->Set(L"headers", headers);
    return dict;
  }

 private:
  ~NetLogHttpRequestParameter() {}

  const std::string line_;
  HttpRequestHeaders headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogHttpRequestParameter);
};

class NetLogHttpResponseParameter : public NetLog::EventParameters {
 public:
  explicit NetLogHttpResponseParameter(
      const scoped_refptr<HttpResponseHeaders>& headers)
      : headers_(headers) {}

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    ListValue* headers = new ListValue();
    headers->Append(new StringValue(headers_->GetStatusLine()));
    void* iterator = NULL;
    std::string name;
    std::string value;
    while (headers_->EnumerateHeaderLines(&iterator, &name, &value)) {
      headers->Append(
          new StringValue(StringPrintf("%s: %s", name.c_str(), value.c_str())));
    }
    dict->Set(L"headers", headers);
    return dict;
  }

 private:
  ~NetLogHttpResponseParameter() {}

  const scoped_refptr<HttpResponseHeaders> headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogHttpResponseParameter);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NET_LOG_PARAMS_H_

