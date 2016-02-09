// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/stats_collection_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "mojo/services/tracing/public/cpp/switches.h"
#include "mojo/shell/public/cpp/shell.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace html_viewer {

namespace {

// Initialize the histogram data using the given startup performance times.
void GetStartupPerformanceTimesCallbackImpl(
    tracing::StartupPerformanceTimesPtr times) {
  base::StatisticsRecorder::Initialize();

  startup_metric_utils::RecordStartupProcessCreationTime(
      base::Time::FromInternalValue(times->shell_process_creation_time));

  // TODO(msw): Record the MojoMain() entry point time of mojo:browser instead?
  startup_metric_utils::RecordMainEntryPointTime(
      base::Time::FromInternalValue(times->shell_main_entry_point_time));

  // TODO(msw): Determine if this is the first run and provide a PrefService
  // to generate stats that span multiple startups.
  startup_metric_utils::RecordBrowserMainMessageLoopStart(
      base::TimeTicks::FromInternalValue(
          times->browser_message_loop_start_ticks),
      false, nullptr);

  startup_metric_utils::RecordBrowserWindowDisplay(
      base::TimeTicks::FromInternalValue(times->browser_window_display_ticks));

  startup_metric_utils::RecordBrowserOpenTabsDelta(
      base::TimeDelta::FromInternalValue(times->browser_open_tabs_time_delta));

  startup_metric_utils::RecordFirstWebContentsMainFrameLoad(
      base::TimeTicks::FromInternalValue(
          times->first_web_contents_main_frame_load_ticks));

  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::FromInternalValue(
          times->first_visually_non_empty_layout_ticks));
}

}  // namespace

// static
gin::WrapperInfo StatsCollectionController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
tracing::StartupPerformanceDataCollectorPtr StatsCollectionController::Install(
    blink::WebFrame* frame,
    mojo::Shell* shell) {
  // Only make startup tracing available when running in the context of a test.
  if (!shell ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    return nullptr;
  }

  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;

  v8::Context::Scope context_scope(context);

  scoped_ptr<mojo::Connection> connection = shell->Connect("mojo:tracing");
  if (!connection)
    return nullptr;
  tracing::StartupPerformanceDataCollectorPtr collector_for_controller;
  tracing::StartupPerformanceDataCollectorPtr collector_for_caller;
  connection->GetInterface(&collector_for_controller);
  connection->GetInterface(&collector_for_caller);

  gin::Handle<StatsCollectionController> controller = gin::CreateHandle(
      isolate,
      new StatsCollectionController(std::move(collector_for_controller)));
  DCHECK(!controller.IsEmpty());
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "statsCollectionController"),
              controller.ToV8());
  return collector_for_caller;
}

// static
tracing::StartupPerformanceDataCollectorPtr
StatsCollectionController::ConnectToDataCollector(mojo::Shell* shell) {
  // Only make startup tracing available when running in the context of a test.
  if (!shell ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    return nullptr;
  }

  tracing::StartupPerformanceDataCollectorPtr collector;
  shell->ConnectToService("mojo:tracing", &collector);
  return collector;
}

StatsCollectionController::StatsCollectionController(
    tracing::StartupPerformanceDataCollectorPtr collector)
    : startup_performance_data_collector_(std::move(collector)) {}

StatsCollectionController::~StatsCollectionController() {}

gin::ObjectTemplateBuilder StatsCollectionController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<StatsCollectionController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getHistogram", &StatsCollectionController::GetHistogram)
      .SetMethod("getBrowserHistogram",
                 &StatsCollectionController::GetBrowserHistogram);
}

std::string StatsCollectionController::GetHistogram(
    const std::string& histogram_name) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      tracing::kEnableStatsCollectionBindings));

  static bool startup_histogram_initialized = false;
  if (!startup_histogram_initialized) {
    // Get the startup performance times from the tracing service.
    auto callback = base::Bind(&GetStartupPerformanceTimesCallbackImpl);
    startup_performance_data_collector_->GetStartupPerformanceTimes(callback);
    startup_performance_data_collector_.WaitForIncomingResponse();
    DCHECK(base::StatisticsRecorder::IsActive());
    startup_histogram_initialized = true;
  }

  std::string histogram_json = "{}";
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  if (histogram)
    histogram->WriteJSON(&histogram_json);
  return histogram_json;
}

std::string StatsCollectionController::GetBrowserHistogram(
    const std::string& histogram_name) {
  return GetHistogram(histogram_name);
}

}  // namespace html_viewer
