// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"

namespace content {

namespace {

const char kRecordUntilFull[]   = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kEnableSampling[] = "enable-sampling";

}  // namespace

DevToolsTracingHandler::DevToolsTracingHandler()
    : is_running_(false) {
  RegisterCommandHandler(devtools::Tracing::start::kName,
                         base::Bind(&DevToolsTracingHandler::OnStart,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Tracing::end::kName,
                         base::Bind(&DevToolsTracingHandler::OnEnd,
                                    base::Unretained(this)));
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

void DevToolsTracingHandler::OnEndTracingComplete() {
  is_running_ = false;
  SendNotification(devtools::Tracing::tracingComplete::kName, NULL);
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  if (is_running_) {
    // Hand-craft protocol notification message so we can substitute JSON
    // that we already got as string as a bare object, not a quoted string.
    std::string message = base::StringPrintf(
        "{ \"method\": \"%s\", \"params\": { \"%s\": [ %s ] } }",
        devtools::Tracing::dataCollected::kName,
        devtools::Tracing::dataCollected::kValue,
        trace_fragment->data().c_str());
    SendRawMessage(message);
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
    } else if (*iter == kRecordContinuously) {
      ret |= base::debug::TraceLog::RECORD_CONTINUOUSLY;
    } else if (*iter == kEnableSampling) {
      ret |= base::debug::TraceLog::ENABLE_SAMPLING;
    }
  }
  if (!(ret & base::debug::TraceLog::RECORD_UNTIL_FULL) &&
      !(ret & base::debug::TraceLog::RECORD_CONTINUOUSLY))
    ret |= base::debug::TraceLog::RECORD_UNTIL_FULL;

  return static_cast<base::debug::TraceLog::Options>(ret);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnStart(
    scoped_refptr<DevToolsProtocol::Command> command) {
  std::string categories;
  base::DictionaryValue* params = command->params();
  if (params)
    params->GetString(devtools::Tracing::start::kCategories, &categories);

  base::debug::TraceLog::Options options =
      base::debug::TraceLog::RECORD_UNTIL_FULL;
  if (params && params->HasKey(devtools::Tracing::start::kTraceOptions)) {
    std::string options_param;
    params->GetString(devtools::Tracing::start::kTraceOptions, &options_param);
    options = TraceOptionsFromString(options_param);
  }

  TraceController::GetInstance()->BeginTracing(this, categories, options);
  is_running_ = true;
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnEnd(
    scoped_refptr<DevToolsProtocol::Command> command) {
  TraceController::GetInstance()->EndTracingAsync(this);
  return command->SuccessResponse(NULL);
}

}  // namespace content
