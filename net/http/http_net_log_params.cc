// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_net_log_params.h"

#include "base/stringprintf.h"
#include "base/values.h"
#include "net/http/http_response_headers.h"

namespace net {

NetLogHttpRequestParameter::NetLogHttpRequestParameter(
    const std::string& line,
    const HttpRequestHeaders& headers)
    : line_(line) {
  headers_.CopyFrom(headers);
}

Value* NetLogHttpRequestParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("line", line_);
  ListValue* headers = new ListValue();
  HttpRequestHeaders::Iterator iterator(headers_);
  while (iterator.GetNext()) {
    headers->Append(
        new StringValue(base::StringPrintf("%s: %s",
                                           iterator.name().c_str(),
                                           iterator.value().c_str())));
  }
  dict->Set("headers", headers);
  return dict;
}

NetLogHttpRequestParameter::~NetLogHttpRequestParameter() {}

NetLogHttpResponseParameter::NetLogHttpResponseParameter(
    const scoped_refptr<HttpResponseHeaders>& headers)
    : headers_(headers) {}

Value* NetLogHttpResponseParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* headers = new ListValue();
  headers->Append(new StringValue(headers_->GetStatusLine()));
  void* iterator = NULL;
  std::string name;
  std::string value;
  while (headers_->EnumerateHeaderLines(&iterator, &name, &value)) {
    headers->Append(
        new StringValue(base::StringPrintf("%s: %s", name.c_str(),
                                           value.c_str())));
  }
  dict->Set("headers", headers);
  return dict;
}

NetLogHttpResponseParameter::~NetLogHttpResponseParameter() {}

}  // namespace net
