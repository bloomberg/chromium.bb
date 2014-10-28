// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <cmath>

#include "base/bind.h"
#include "base/debug/trace_event_impl.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {
namespace devtools {
namespace tracing {

typedef DevToolsProtocolClient::Response Response;

namespace {

const char kRecordUntilFull[]   = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kRecordAsMuchAsPossible[] = "record-as-much-as-possible";
const char kEnableSampling[] = "enable-sampling";
const double kMinimumReportingInterval = 250.0;

class DevToolsTraceSinkProxy : public TracingController::TraceDataSink {
 public:
  explicit DevToolsTraceSinkProxy(base::WeakPtr<TracingHandler> handler)
      : tracing_handler_(handler) {}

  void AddTraceChunk(const std::string& chunk) override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceDataCollected(chunk);
  }
  void Close() override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceComplete();
  }

 private:
  ~DevToolsTraceSinkProxy() override {}

  base::WeakPtr<TracingHandler> tracing_handler_;
};

}  // namespace

TracingHandler::TracingHandler(TracingHandler::Target target)
    : target_(target),
      is_recording_(false),
      weak_factory_(this) {
}

TracingHandler::~TracingHandler() {
}

void TracingHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void TracingHandler::Detached() {
  if (is_recording_)
    DisableRecording(true);
}

void TracingHandler::OnTraceDataCollected(const std::string& trace_fragment) {
  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message(
      "{ \"method\": \"Tracing.dataCollected\", \"params\": { \"value\": [");
  const size_t messageSuffixSize = 10;
  message.reserve(message.size() + trace_fragment.size() + messageSuffixSize);
  message += trace_fragment;
  message += "] } }";
  client_->SendRawMessage(message);
}

void TracingHandler::OnTraceComplete() {
  TracingCompleteParams params;
  client_->TracingComplete(params);
}

scoped_refptr<DevToolsProtocol::Response> TracingHandler::Start(
    const std::string* categories,
    const std::string* options_str,
    const double* buffer_usage_reporting_interval,
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (is_recording_)
    return command->InternalErrorResponse("Tracing is already started");
  is_recording_ = true;

  base::debug::TraceOptions options = TraceOptionsFromString(options_str);
  base::debug::CategoryFilter filter(categories ? *categories : std::string());
  if (buffer_usage_reporting_interval)
    SetupTimer(*buffer_usage_reporting_interval);

  // If inspected target is a render process Tracing.start will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer) {
    TracingController::GetInstance()->EnableRecording(
        filter,
        options,
        TracingController::EnableRecordingDoneCallback());
    return nullptr;
  }

  TracingController::GetInstance()->EnableRecording(
      filter,
      options,
      base::Bind(&TracingHandler::OnRecordingEnabled,
                 weak_factory_.GetWeakPtr(),
                 command));
  return command->AsyncResponsePromise();
}

scoped_refptr<DevToolsProtocol::Response> TracingHandler::End(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!is_recording_)
    return command->InternalErrorResponse("Tracing is not started");
  DisableRecording(false);
  // If inspected target is a render process Tracing.end will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer)
    return nullptr;
  return command->SuccessResponse(nullptr);
}

scoped_refptr<DevToolsProtocol::Response> TracingHandler::GetCategories(
    scoped_refptr<DevToolsProtocol::Command> command) {
  TracingController::GetInstance()->GetCategories(
      base::Bind(&TracingHandler::OnCategoriesReceived,
                 weak_factory_.GetWeakPtr(),
                 command));
  return command->AsyncResponsePromise();
}

void TracingHandler::OnRecordingEnabled(
    scoped_refptr<DevToolsProtocol::Command> command) {
  StartResponse response;
  client_->SendStartResponse(command, response);
}

void TracingHandler::OnBufferUsage(float usage) {
  BufferUsageParams params;
  params.set_value(usage);
  client_->BufferUsage(params);
}

void TracingHandler::OnCategoriesReceived(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::set<std::string>& category_set) {
  std::vector<std::string> categories(category_set.begin(), category_set.end());
  GetCategoriesResponse response;
  response.set_categories(categories);
  client_->SendGetCategoriesResponse(command, response);
}

base::debug::TraceOptions TracingHandler::TraceOptionsFromString(
    const std::string* options) {
  base::debug::TraceOptions ret;
  if (!options)
    return ret;

  std::vector<std::string> split;
  std::vector<std::string>::iterator iter;

  base::SplitString(*options, ',', &split);
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

void TracingHandler::SetupTimer(double usage_reporting_interval) {
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
          base::Bind(&TracingHandler::OnBufferUsage,
                     weak_factory_.GetWeakPtr())),
      true));
  buffer_usage_poll_timer_->Reset();
}

void TracingHandler::DisableRecording(bool abort) {
  is_recording_ = false;
  buffer_usage_poll_timer_.reset();
  TracingController::GetInstance()->DisableRecording(
      abort ? nullptr : new DevToolsTraceSinkProxy(weak_factory_.GetWeakPtr()));
}

}  // namespace tracing
}  // namespace devtools
}  // namespace content
