// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <cmath>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/tracing_agent.h"
#include "components/tracing/trace_config_file.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/tracing/tracing_controller_impl.h"

namespace content {
namespace devtools {
namespace tracing {

using Response = DevToolsProtocolClient::Response;

namespace {

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

class DevToolsStreamTraceSink : public TracingController::TraceDataSink {
 public:
  explicit DevToolsStreamTraceSink(
      base::WeakPtr<TracingHandler> handler,
      const scoped_refptr<DevToolsIOContext::Stream>& stream)
      : stream_(stream),
        tracing_handler_(handler),
        first_chunk_(true) {}

  void AddTraceChunk(const std::string& chunk) override {
    // FIXME: change interface to pass chunks as refcounted strings.
    scoped_refptr<base::RefCountedString> ref_counted_chunk
        = new base::RefCountedString();
    std::string prefix = first_chunk_ ? "[" : ",";
    ref_counted_chunk->data() = prefix + chunk;
    first_chunk_ = false;
    stream_->Append(ref_counted_chunk);
  }

  void Close() override {
    if (TracingHandler* h = tracing_handler_.get()) {
      std::string suffix = "]";
      stream_->Append(base::RefCountedString::TakeString(&suffix));
      h->OnTraceToStreamComplete(stream_->handle());
    }
  }

 private:
  ~DevToolsStreamTraceSink() override {}

  scoped_refptr<DevToolsIOContext::Stream> stream_;
  base::WeakPtr<TracingHandler> tracing_handler_;
  bool first_chunk_;
};

}  // namespace

TracingHandler::TracingHandler(TracingHandler::Target target,
                               DevToolsIOContext* io_context)
    : target_(target),
      io_context_(io_context),
      did_initiate_recording_(false),
      return_as_stream_(false),
      weak_factory_(this) {}

TracingHandler::~TracingHandler() {
}

void TracingHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void TracingHandler::Detached() {
  if (did_initiate_recording_)
    StopTracing(scoped_refptr<TracingController::TraceDataSink>());
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
  client_->SendRawNotification(message);
}

void TracingHandler::OnTraceComplete() {
  client_->TracingComplete(TracingCompleteParams::Create());
}

void TracingHandler::OnTraceToStreamComplete(const std::string& stream_handle) {
  client_->TracingComplete(
      TracingCompleteParams::Create()->set_stream(stream_handle));
}

Response TracingHandler::Start(DevToolsCommandId command_id,
                               const std::string* categories,
                               const std::string* options,
                               const double* buffer_usage_reporting_interval,
                               const std::string* transfer_mode) {
  if (IsTracing())
    return Response::InternalError("Tracing is already started");

  did_initiate_recording_ = true;
  return_as_stream_ =
      transfer_mode && *transfer_mode == start::kTransferModeReturnAsStream;
  base::trace_event::TraceConfig trace_config(
      categories ? *categories : std::string(),
      options ? *options : std::string());
  if (buffer_usage_reporting_interval)
    SetupTimer(*buffer_usage_reporting_interval);

  // If inspected target is a render process Tracing.start will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer) {
    TracingController::GetInstance()->StartTracing(
        trace_config,
        TracingController::StartTracingDoneCallback());
    return Response::FallThrough();
  }

  TracingController::GetInstance()->StartTracing(
      trace_config,
      base::Bind(&TracingHandler::OnRecordingEnabled,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

Response TracingHandler::End(DevToolsCommandId command_id) {
  // Startup tracing triggered by --trace-config-file is a special case, where
  // tracing is started automatically upon browser startup and can be stopped
  // via DevTools.
  if (!did_initiate_recording_ && !IsStartupTracingActive())
    return Response::InternalError("Tracing is not started");

  scoped_refptr<TracingController::TraceDataSink> proxy;
  if (return_as_stream_) {
    proxy = new DevToolsStreamTraceSink(
        weak_factory_.GetWeakPtr(), io_context_->CreateTempFileBackedStream());
  } else {
    proxy = new DevToolsTraceSinkProxy(weak_factory_.GetWeakPtr());
  }
  StopTracing(proxy);
  // If inspected target is a render process Tracing.end will be handled by
  // tracing agent in the renderer.
  return target_ == Renderer ? Response::FallThrough() : Response::OK();
}

Response TracingHandler::GetCategories(DevToolsCommandId command_id) {
  TracingController::GetInstance()->GetCategories(
      base::Bind(&TracingHandler::OnCategoriesReceived,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

void TracingHandler::OnRecordingEnabled(DevToolsCommandId command_id) {
  client_->SendStartResponse(command_id, StartResponse::Create());
}

void TracingHandler::OnBufferUsage(float percent_full,
                                   size_t approximate_event_count) {
  // TODO(crbug426117): remove set_value once all clients have switched to
  // the new interface of the event.
  client_->BufferUsage(BufferUsageParams::Create()
                           ->set_value(percent_full)
                           ->set_percent_full(percent_full)
                           ->set_event_count(approximate_event_count));
}

void TracingHandler::OnCategoriesReceived(
    DevToolsCommandId command_id,
    const std::set<std::string>& category_set) {
  std::vector<std::string> categories;
  for (const std::string& category : category_set)
    categories.push_back(category);
  client_->SendGetCategoriesResponse(command_id,
      GetCategoriesResponse::Create()->set_categories(categories));
}

Response TracingHandler::RequestMemoryDump(DevToolsCommandId command_id) {
  if (!IsTracing())
    return Response::InternalError("Tracing is not started");

  base::trace_event::MemoryDumpManager::GetInstance()->RequestGlobalDump(
      base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
      base::Bind(&TracingHandler::OnMemoryDumpFinished,
                 weak_factory_.GetWeakPtr(), command_id));
  return Response::OK();
}

void TracingHandler::OnMemoryDumpFinished(DevToolsCommandId command_id,
                                          uint64_t dump_guid,
                                          bool success) {
  client_->SendRequestMemoryDumpResponse(
      command_id,
      RequestMemoryDumpResponse::Create()
          ->set_dump_guid(base::StringPrintf("0x%" PRIx64, dump_guid))
          ->set_success(success));
}

Response TracingHandler::RecordClockSyncMarker(const std::string& sync_id) {
  if (!IsTracing())
    return Response::InternalError("Tracing is not started");

  TracingControllerImpl::GetInstance()->RecordClockSyncMarker(
      sync_id,
      base::trace_event::TracingAgent::RecordClockSyncMarkerCallback());

  return Response::OK();
}

void TracingHandler::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0) return;

  if (usage_reporting_interval < kMinimumReportingInterval)
      usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_.reset(new base::Timer(
      FROM_HERE, interval,
      base::Bind(base::IgnoreResult(&TracingController::GetTraceBufferUsage),
                 base::Unretained(TracingController::GetInstance()),
                 base::Bind(&TracingHandler::OnBufferUsage,
                            weak_factory_.GetWeakPtr())),
      true));
  buffer_usage_poll_timer_->Reset();
}

void TracingHandler::StopTracing(
    const scoped_refptr<TracingController::TraceDataSink>& trace_data_sink) {
  buffer_usage_poll_timer_.reset();
  TracingController::GetInstance()->StopTracing(trace_data_sink);
  did_initiate_recording_ = false;
}

bool TracingHandler::IsTracing() const {
  return TracingController::GetInstance()->IsTracing();
}

bool TracingHandler::IsStartupTracingActive() {
  return ::tracing::TraceConfigFile::GetInstance()->IsEnabled() &&
      TracingController::GetInstance()->IsTracing();
}

}  // namespace tracing
}  // namespace devtools
}  // namespace content
