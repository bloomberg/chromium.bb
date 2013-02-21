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

const char kTracingStartCommand[] = "Tracing.start";
const char kTracingEndCommand[] = "Tracing.end";

const char kTracingCompleteNotification[] = "Tracing.tracingComplete";
const char kTracingDataCollected[] = "Tracing.dataCollected";

const char kCategoriesParam[] = "categories";

}  // namespace

const char DevToolsTracingHandler::kDomain[] = "Tracing";

DevToolsTracingHandler::DevToolsTracingHandler()
    : is_running_(false) {
  RegisterCommandHandler(kTracingStartCommand,
                         base::Bind(&DevToolsTracingHandler::OnStart,
                                    base::Unretained(this)));
  RegisterCommandHandler(kTracingEndCommand,
                         base::Bind(&DevToolsTracingHandler::OnEnd,
                                    base::Unretained(this)));
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

void DevToolsTracingHandler::OnEndTracingComplete() {
  is_running_ = false;
  SendNotification(kTracingCompleteNotification, NULL);
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  if (is_running_) {
    base::DictionaryValue* params = new base::DictionaryValue();
    params->SetString("value", trace_fragment->data());
    SendNotification(kTracingDataCollected, params);
  }
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnStart(DevToolsProtocol::Command* command) {
  std::string categories;
  base::DictionaryValue* params = command->params();
  if (params && params->HasKey(kCategoriesParam))
    params->GetString(kCategoriesParam, &categories);
  TraceController::GetInstance()->BeginTracing(this, categories);
  is_running_ = true;
  return command->SuccessResponse(NULL);
}


scoped_ptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnEnd(DevToolsProtocol::Command* command) {
  TraceController::GetInstance()->EndTracingAsync(this);
  return command->SuccessResponse(NULL);
}

}  // namespace content
