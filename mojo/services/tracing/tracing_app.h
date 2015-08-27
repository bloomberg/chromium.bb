// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TRACING_TRACING_APP_H_
#define MOJO_SERVICES_TRACING_TRACING_APP_H_

#include "base/memory/scoped_vector.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"

namespace tracing {

class CollectorImpl;
class TraceDataSink;

class TracingApp
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<TraceCoordinator>,
      public TraceCoordinator,
      public mojo::InterfaceFactory<StartupPerformanceDataCollector>,
      public StartupPerformanceDataCollector {
 public:
  TracingApp();
  ~TracingApp() override;

 private:
  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<TraceCoordinator> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<TraceCoordinator> request) override;

  // mojo::InterfaceFactory<StartupPerformanceDataCollector> implementation.
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<StartupPerformanceDataCollector> request) override;

  // TraceCoordinator implementation.
  void Start(mojo::ScopedDataPipeProducerHandle stream,
             const mojo::String& categories) override;
  void StopAndFlush() override;

  // StartupPerformanceDataCollector implementation.
  void SetShellProcessCreationTime(int64 time) override;
  void SetShellMainEntryPointTime(int64 time) override;
  void SetBrowserMessageLoopStartTime(int64 time) override;
  void SetBrowserWindowDisplayTime(int64 time) override;
  void SetBrowserOpenTabsTimeDelta(int64 delta) override;
  void SetFirstWebContentsMainFrameLoadTime(int64 time) override;
  void SetFirstVisuallyNonEmptyLayoutTime(int64 time) override;
  void GetStartupPerformanceTimes(
      const GetStartupPerformanceTimesCallback& callback) override;

  void AllDataCollected();

  scoped_ptr<TraceDataSink> sink_;
  ScopedVector<CollectorImpl> collector_impls_;
  mojo::WeakInterfacePtrSet<TraceController> controller_ptrs_;
  mojo::WeakBindingSet<TraceCoordinator> coordinator_bindings_;
  mojo::WeakBindingSet<StartupPerformanceDataCollector>
      startup_performance_data_collector_bindings_;
  StartupPerformanceTimes startup_performance_times_;

  DISALLOW_COPY_AND_ASSIGN(TracingApp);
};

}  // namespace tracing

#endif  // MOJO_SERVICES_TRACING_TRACING_APP_H_
