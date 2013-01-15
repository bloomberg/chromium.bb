// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"

namespace content {

DevToolsTracingHandler::DevToolsTracingHandler()
    : is_running_(false) {
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

std::string DevToolsTracingHandler::Domain() {
  return "Tracing";
}

void DevToolsTracingHandler::OnEndTracingComplete() {
  is_running_ = false;
  SendNotification("Tracing.tracingComplete", "");
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  if (is_running_)
    SendNotification("Tracing.dataCollected", trace_fragment->data());
}

base::Value* DevToolsTracingHandler::OnProtocolCommand(
    const std::string& method,
    const base::DictionaryValue* params,
    base::Value** error_out) {
  if (method == "Tracing.start")
    return Start(params);
  else if (method == "Tracing.end")
    return End(params);

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
  is_running_ = true;

  return base::Value::CreateBooleanValue(true);
}

base::Value* DevToolsTracingHandler::End(
    const base::DictionaryValue* /* params */) {
  TraceController::GetInstance()->EndTracingAsync(this);

  return base::Value::CreateBooleanValue(true);
}

void DevToolsTracingHandler::SendNotification(
    const std::string& method, const std::string& value) {
  scoped_ptr<base::DictionaryValue> ret(new base::DictionaryValue());
  ret->SetString("method", method);

  if (!value.empty()) {
    base::DictionaryValue* params = new base::DictionaryValue();
    params->SetString("value", value);

    ret->Set("params", params);
  }

  // Serialize response.
  std::string json_response;
  base::JSONWriter::Write(ret.get(), &json_response);

  notifier()->Notify(json_response);
}


}  // namespace content
