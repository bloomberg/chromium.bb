// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/tracing/tracing_app.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/services/tracing/trace_data_sink.h"

namespace tracing {

class CollectorImpl : public TraceDataCollector {
 public:
  CollectorImpl(mojo::InterfaceRequest<TraceDataCollector> request,
                TraceDataSink* sink)
      : sink_(sink), binding_(this, request.Pass()) {}

  ~CollectorImpl() override {}

  // tracing::TraceDataCollector implementation.
  void DataCollected(const mojo::String& json) override {
    sink_->AddChunk(json.To<std::string>());
  }

 private:
  TraceDataSink* sink_;
  mojo::Binding<TraceDataCollector> binding_;

  DISALLOW_COPY_AND_ASSIGN(CollectorImpl);
};

TracingApp::TracingApp() {}

TracingApp::~TracingApp() {}

bool TracingApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<TraceCoordinator>(this);
  connection->AddService<StartupPerformanceDataCollector>(this);

  // If someone connects to us they may want to use the TraceCoordinator
  // interface and/or they may want to expose themselves to be traced. Attempt
  // to connect to the TraceController interface to see if the application
  // connecting to us wants to be traced. They can refuse the connection or
  // close the pipe if not.
  TraceControllerPtr controller_ptr;
  connection->ConnectToService(&controller_ptr);
  if (controller_ptr)
    controller_ptrs_.AddInterfacePtr(controller_ptr.Pass());
  return true;
}

void TracingApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<TraceCoordinator> request) {
  coordinator_bindings_.AddBinding(this, request.Pass());
}

void TracingApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<StartupPerformanceDataCollector> request) {
  startup_performance_data_collector_bindings_.AddBinding(this, request.Pass());
}

void TracingApp::Start(mojo::ScopedDataPipeProducerHandle stream,
                       const mojo::String& categories) {
  sink_.reset(new TraceDataSink(stream.Pass()));
  controller_ptrs_.ForAllPtrs(
      [categories, this](TraceController* controller) {
        TraceDataCollectorPtr ptr;
        collector_impls_.push_back(
            new CollectorImpl(GetProxy(&ptr), sink_.get()));
        controller->StartTracing(categories, ptr.Pass());
      });
}

void TracingApp::StopAndFlush() {
  controller_ptrs_.ForAllPtrs(
      [](TraceController* controller) { controller->StopTracing(); });

  // TODO: We really should keep track of how many connections we have here
  // and flush + reset the sink after we receive a EndTracing or a detect a
  // pipe closure on all pipes.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TracingApp::AllDataCollected, base::Unretained(this)),
      base::TimeDelta::FromSeconds(1));
}

void TracingApp::SetShellProcessCreationTime(int64 time) {
  if (startup_performance_times_.shell_process_creation_time == 0)
    startup_performance_times_.shell_process_creation_time = time;
}

void TracingApp::SetShellMainEntryPointTime(int64 time) {
  if (startup_performance_times_.shell_main_entry_point_time == 0)
    startup_performance_times_.shell_main_entry_point_time = time;
}

void TracingApp::SetBrowserMessageLoopStartTime(int64 time) {
  if (startup_performance_times_.browser_message_loop_start_time == 0)
    startup_performance_times_.browser_message_loop_start_time = time;
}

void TracingApp::SetBrowserWindowDisplayTime(int64 time) {
  if (startup_performance_times_.browser_window_display_time == 0)
    startup_performance_times_.browser_window_display_time = time;
}

void TracingApp::SetBrowserOpenTabsTimeDelta(int64 delta) {
  if (startup_performance_times_.browser_open_tabs_time_delta == 0)
    startup_performance_times_.browser_open_tabs_time_delta = delta;
}

void TracingApp::SetFirstWebContentsMainFrameLoadTime(int64 time) {
  if (startup_performance_times_.first_web_contents_main_frame_load_time == 0)
    startup_performance_times_.first_web_contents_main_frame_load_time = time;
}

void TracingApp::SetFirstVisuallyNonEmptyLayoutTime(int64 time) {
  if (startup_performance_times_.first_visually_non_empty_layout_time == 0)
    startup_performance_times_.first_visually_non_empty_layout_time = time;
}

void TracingApp::GetStartupPerformanceTimes(
    const GetStartupPerformanceTimesCallback& callback) {
  callback.Run(startup_performance_times_.Clone());
}

void TracingApp::AllDataCollected() {
  collector_impls_.clear();
  sink_->Flush();
}

}  // namespace tracing
