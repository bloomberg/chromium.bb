// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/string_split.h"
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

const char kTraceOptionsParam[] = "trace-options";
const char kRecordUntilFull[]   = "record-until-full";

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

// Note, if you add more options here you also need to update:
// base/debug/trace_event_impl:TraceOptionsFromString
base::debug::TraceLog::Options DevToolsTracingHandler::TraceOptionsFromString(
    const std::string& options) {
  std::vector<std::string> split;
  std::vector<std::string>::iterator iter;
  int ret = 0;

  base::SplitString(options, ',', &split);
  for (iter = split.begin(); iter != split.end(); ++iter) {
    if (*iter == kRecordUntilFull) {
      ret |= base::debug::TraceLog::RECORD_UNTIL_FULL;
    }
  }
  if (ret == 0)
    ret = base::debug::TraceLog::RECORD_UNTIL_FULL;

  return static_cast<base::debug::TraceLog::Options>(ret);
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnStart(DevToolsProtocol::Command* command) {
  std::string categories;
  base::DictionaryValue* params = command->params();
  if (params && params->HasKey(kCategoriesParam))
    params->GetString(kCategoriesParam, &categories);

  base::debug::TraceLog::Options options =
      base::debug::TraceLog::RECORD_UNTIL_FULL;
  if (params && params->HasKey(kTraceOptionsParam)) {
    std::string options_param;
    params->GetString(kTraceOptionsParam, &options_param);
    options = TraceOptionsFromString(options_param);
  }

  TraceController::GetInstance()->BeginTracing(this, categories, options);
  is_running_ = true;
  return command->SuccessResponse(NULL);
}


scoped_ptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnEnd(DevToolsProtocol::Command* command) {
  TraceController::GetInstance()->EndTracingAsync(this);
  return command->SuccessResponse(NULL);
}

}  // namespace content
