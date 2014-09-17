// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_tracing_handler.h"

#include <cmath>

#include "base/bind.h"
#include "base/debug/trace_event_impl.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"

namespace content {

namespace {

const char kRecordUntilFull[]   = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kRecordAsMuchAsPossible[] = "record-as-much-as-possible";
const char kEnableSampling[] = "enable-sampling";

class DevToolsTraceSinkProxy : public TracingController::TraceDataSink {
 public:
  explicit DevToolsTraceSinkProxy(base::WeakPtr<DevToolsTracingHandler> handler)
      : tracing_handler_(handler) {}

  virtual void AddTraceChunk(const std::string& chunk) OVERRIDE {
    if (DevToolsTracingHandler* h = tracing_handler_.get())
      h->OnTraceDataCollected(chunk);
  }
  virtual void Close() OVERRIDE {
    if (DevToolsTracingHandler* h = tracing_handler_.get())
      h->OnTraceComplete();
  }

 private:
  virtual ~DevToolsTraceSinkProxy() {}

  base::WeakPtr<DevToolsTracingHandler> tracing_handler_;
};

}  // namespace

const char* DevToolsTracingHandler::kDefaultCategories =
    "-*,disabled-by-default-devtools.timeline*";
const double DevToolsTracingHandler::kDefaultReportingInterval = 1000.0;
const double DevToolsTracingHandler::kMinimumReportingInterval = 250.0;

DevToolsTracingHandler::DevToolsTracingHandler(
    DevToolsTracingHandler::Target target)
    : target_(target), is_recording_(false), weak_factory_(this) {
  RegisterCommandHandler(devtools::Tracing::start::kName,
                         base::Bind(&DevToolsTracingHandler::OnStart,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Tracing::end::kName,
                         base::Bind(&DevToolsTracingHandler::OnEnd,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Tracing::getCategories::kName,
                         base::Bind(&DevToolsTracingHandler::OnGetCategories,
                                    base::Unretained(this)));
}

DevToolsTracingHandler::~DevToolsTracingHandler() {
}

void DevToolsTracingHandler::OnTraceDataCollected(
    const std::string& trace_fragment) {
  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message =
      base::StringPrintf("{ \"method\": \"%s\", \"params\": { \"%s\": [",
                         devtools::Tracing::dataCollected::kName,
                         devtools::Tracing::dataCollected::kParamValue);
  const size_t messageSuffixSize = 10;
  message.reserve(message.size() + trace_fragment.size() + messageSuffixSize);
  message += trace_fragment;
  message += "] } }", SendRawMessage(message);
}

void DevToolsTracingHandler::OnTraceComplete() {
  SendNotification(devtools::Tracing::tracingComplete::kName, NULL);
}

base::debug::TraceOptions DevToolsTracingHandler::TraceOptionsFromString(
    const std::string& options) {
  std::vector<std::string> split;
  std::vector<std::string>::iterator iter;
  base::debug::TraceOptions ret;

  base::SplitString(options, ',', &split);
  for (iter = split.begin(); iter != split.end(); ++iter) {
    if (*iter == kRecordUntilFull) {
      ret.record_mode = base::debug::RECORD_UNTIL_FULL;
    } else if (*iter == kRecordContinuously) {
      ret.record_mode = base::debug::RECORD_CONTINUOUSLY;
    } else if (*iter == kRecordAsMuchAsPossible) {
      ret.record_mode = base::debug::RECORD_AS_MUCH_AS_POSSIBLE;
    } else if (*iter == kEnableSampling) {
      ret.enable_sampling = true;
    }
  }
  return ret;
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnStart(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (is_recording_) {
    return command->InternalErrorResponse("Tracing is already started");
  }
  is_recording_ = true;

  std::string categories;
  base::debug::TraceOptions options;
  double usage_reporting_interval = 0.0;

  base::DictionaryValue* params = command->params();
  if (params) {
    params->GetString(devtools::Tracing::start::kParamCategories, &categories);
    std::string options_param;
    if (params->GetString(devtools::Tracing::start::kParamOptions,
                          &options_param)) {
      options = TraceOptionsFromString(options_param);
    }
    params->GetDouble(
        devtools::Tracing::start::kParamBufferUsageReportingInterval,
        &usage_reporting_interval);
  }

  SetupTimer(usage_reporting_interval);

  // If inspected target is a render process Tracing.start will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer) {
    TracingController::GetInstance()->EnableRecording(
        base::debug::CategoryFilter(categories),
        options,
        TracingController::EnableRecordingDoneCallback());
    return NULL;
  }

  TracingController::GetInstance()->EnableRecording(
      base::debug::CategoryFilter(categories),
      options,
      base::Bind(&DevToolsTracingHandler::OnRecordingEnabled,
                 weak_factory_.GetWeakPtr(),
                 command));
  return command->AsyncResponsePromise();
}

void DevToolsTracingHandler::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0) return;

  if (usage_reporting_interval < kMinimumReportingInterval)
      usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_.reset(new base::Timer(
      FROM_HERE,
      interval,
      base::Bind(
          base::IgnoreResult(&TracingController::GetTraceBufferPercentFull),
          base::Unretained(TracingController::GetInstance()),
          base::Bind(&DevToolsTracingHandler::OnBufferUsage,
                     weak_factory_.GetWeakPtr())),
      true));
  buffer_usage_poll_timer_->Reset();
}

void DevToolsTracingHandler::OnRecordingEnabled(
    scoped_refptr<DevToolsProtocol::Command> command) {
  SendAsyncResponse(command->SuccessResponse(NULL));
}

void DevToolsTracingHandler::OnBufferUsage(float usage) {
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetDouble(devtools::Tracing::bufferUsage::kParamValue, usage);
  SendNotification(devtools::Tracing::bufferUsage::kName, params);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnEnd(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!is_recording_) {
    return command->InternalErrorResponse("Tracing is not started");
  }
  DisableRecording(false);
  // If inspected target is a render process Tracing.end will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer)
    return NULL;
  return command->SuccessResponse(NULL);
}

void DevToolsTracingHandler::DisableRecording(bool abort) {
  is_recording_ = false;
  buffer_usage_poll_timer_.reset();
  TracingController::GetInstance()->DisableRecording(
      abort ? NULL : new DevToolsTraceSinkProxy(weak_factory_.GetWeakPtr()));
}

void DevToolsTracingHandler::OnClientDetached() {
  if (is_recording_)
    DisableRecording(true);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsTracingHandler::OnGetCategories(
    scoped_refptr<DevToolsProtocol::Command> command) {
  TracingController::GetInstance()->GetCategories(
      base::Bind(&DevToolsTracingHandler::OnCategoriesReceived,
                 weak_factory_.GetWeakPtr(),
                 command));
  return command->AsyncResponsePromise();
}

void DevToolsTracingHandler::OnCategoriesReceived(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::set<std::string>& category_set) {
  base::DictionaryValue* response = new base::DictionaryValue;
  base::ListValue* category_list = new base::ListValue;
  for (std::set<std::string>::const_iterator it = category_set.begin();
       it != category_set.end(); ++it) {
    category_list->AppendString(*it);
  }

  response->Set(devtools::Tracing::getCategories::kResponseCategories,
                category_list);
  SendAsyncResponse(command->SuccessResponse(response));
}

}  // namespace content
