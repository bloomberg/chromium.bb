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

namespace {

const char kTracingDomain[] = "Tracing";

const char kTracingStartCommand[] = "Tracing.start";
const char kTracingEndCommand[] = "Tracing.end";

const char kTracingCompleteNotification[] = "Tracing.tracingComplete";
const char kTracingDataCollected[] = "Tracing.dataCollected";

const char kCategoriesParam[] = "categories";

}  // namespace

DevToolsTracingHandler::DevToolsTracingHandler()
    : DevToolsBrowserTarget::DomainHandler(kTracingDomain),
      is_running_(false) {
  RegisterCommandHandler(kTracingStartCommand,
                         Bind(&DevToolsTracingHandler::OnStart,
                              base::Unretained(this)));
  RegisterCommandHandler(kTracingEndCommand,
                         Bind(&DevToolsTracingHandler::OnEnd,
                              base::Unretained(this)));
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

void DevToolsTracingHandler::OnEndTracingComplete() {
  is_running_ = false;
  SendNotification(kTracingCompleteNotification, NULL, NULL);
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  if (is_running_) {
    base::DictionaryValue* params = new base::DictionaryValue();
    params->SetString("value", trace_fragment->data());
    SendNotification(kTracingDataCollected, params, NULL);
  }
}

base::DictionaryValue* DevToolsTracingHandler::OnStart(
    const base::DictionaryValue* params,
    base::Value** error_out) {
  std::string categories;
  if (params && params->HasKey(kCategoriesParam))
    params->GetString(kCategoriesParam, &categories);
  TraceController::GetInstance()->BeginTracing(this, categories);
  is_running_ = true;
  return NULL;
}

base::DictionaryValue* DevToolsTracingHandler::OnEnd(
    const base::DictionaryValue* params,
    base::Value** error_out) {
  TraceController::GetInstance()->EndTracingAsync(this);
  return NULL;
}

}  // namespace content
