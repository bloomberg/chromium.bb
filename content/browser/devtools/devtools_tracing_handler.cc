// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"

namespace content {

DevToolsTracingHandler::DevToolsTracingHandler()
    : has_completed_(false),
      buffer_data_size_(0) {
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

std::string DevToolsTracingHandler::Domain() {
  return "Tracing";
}

void DevToolsTracingHandler::OnEndTracingComplete() {
  has_completed_ = true;
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  buffer_.push_back(trace_fragment->data());
  buffer_data_size_ += trace_fragment->data().size();
}

base::Value* DevToolsTracingHandler::OnProtocolCommand(
    const std::string& method,
    const base::DictionaryValue* params,
    base::Value** error_out) {
  if (method == "Tracing.start")
    return Start(params);
  else if (method == "Tracing.end")
    return End(params);
  else if (method == "Tracing.hasCompleted")
    return HasCompleted(params);
  else if (method == "Tracing.getTraceAndReset")
    return GetTraceAndReset(params);

  base::DictionaryValue* error_object = new base::DictionaryValue();
  error_object->SetInteger("code", -1);
  error_object->SetString("message", "Invalid method");

  *error_out = error_object;

  return NULL;
}

base::Value* DevToolsTracingHandler::Start(
    const base::DictionaryValue* params) {
  std::string categories;
  if (params && params->HasKey("categories"))
    params->GetString("categories", &categories);
  TraceController::GetInstance()->BeginTracing(this, categories);

  return base::Value::CreateBooleanValue(true);
}

base::Value* DevToolsTracingHandler::End(
    const base::DictionaryValue* /* params */) {
  TraceController::GetInstance()->EndTracingAsync(this);

  return base::Value::CreateBooleanValue(true);
}


base::Value* DevToolsTracingHandler::HasCompleted(
    const base::DictionaryValue* /* params */) {

  return base::Value::CreateBooleanValue(has_completed_);
}

base::Value* DevToolsTracingHandler::GetTraceAndReset(
    const base::DictionaryValue* /* params */) {
  std::string ret;
  ret.reserve(buffer_data_size_);
  for (std::vector<std::string>::const_iterator i = buffer_.begin();
       i != buffer_.end(); ++i) {
    if (!ret.empty())
      ret.append(",");
    ret.append(*i);
  }
  buffer_.clear();
  has_completed_ = false;
  buffer_data_size_ = 0;

  return base::Value::CreateStringValue(ret);
}

}  // namespace content
