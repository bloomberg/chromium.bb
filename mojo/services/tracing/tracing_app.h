// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TRACING_TRACING_APP_H_
#define MOJO_SERVICES_TRACING_TRACING_APP_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "mojo/services/tracing/trace_data_sink.h"
#include "mojo/services/tracing/trace_recorder_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace tracing {

class TracingApp
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<TraceCollector>,
      public TraceCollector,
      public mojo::InterfaceFactory<StartupPerformanceDataCollector>,
      public StartupPerformanceDataCollector {
 public:
  TracingApp();
  ~TracingApp() override;

 private:
  // mojo::ShellClient implementation.
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<TraceCollector> implementation.
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<TraceCollector> request) override;

  // mojo::InterfaceFactory<StartupPerformanceDataCollector> implementation.
  void Create(
      mojo::Connection* connection,
      mojo::InterfaceRequest<StartupPerformanceDataCollector> request) override;

  // tracing::TraceCollector implementation.
  void Start(mojo::ScopedDataPipeProducerHandle stream,
             const mojo::String& categories) override;
  void StopAndFlush() override;

  // StartupPerformanceDataCollector implementation.
  void SetShellProcessCreationTime(int64_t time) override;
  void SetShellMainEntryPointTime(int64_t time) override;
  void SetBrowserMessageLoopStartTicks(int64_t ticks) override;
  void SetBrowserWindowDisplayTicks(int64_t ticks) override;
  void SetBrowserOpenTabsTimeDelta(int64_t delta) override;
  void SetFirstWebContentsMainFrameLoadTicks(int64_t ticks) override;
  void SetFirstVisuallyNonEmptyLayoutTicks(int64_t ticks) override;
  void GetStartupPerformanceTimes(
      const GetStartupPerformanceTimesCallback& callback) override;

  void AllDataCollected();

  scoped_ptr<TraceDataSink> sink_;
  ScopedVector<TraceRecorderImpl> recorder_impls_;
  mojo::WeakInterfacePtrSet<TraceProvider> provider_ptrs_;
  mojo::Binding<TraceCollector> collector_binding_;
  mojo::WeakBindingSet<StartupPerformanceDataCollector>
      startup_performance_data_collector_bindings_;
  StartupPerformanceTimes startup_performance_times_;
  bool tracing_active_;
  mojo::String tracing_categories_;

  DISALLOW_COPY_AND_ASSIGN(TracingApp);
};

}  // namespace tracing

#endif  // MOJO_SERVICES_TRACING_TRACING_APP_H_
