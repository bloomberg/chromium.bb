// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/tracing/trace_data_sink.h"
#include "mojo/services/tracing/tracing.mojom.h"

namespace tracing {

namespace {

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

}  // namespace

class TracingApp : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<TraceCoordinator>,
                   public TraceCoordinator {
 public:
  TracingApp() {}
  ~TracingApp() override {}

 private:
  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<TraceCoordinator>(this);

    // If someone connects to us they may want to use the TraceCoordinator
    // interface and/or they may want to expose themselves to be traced. Attempt
    // to connect to the TraceController interface to see if the application
    // connecting to us wants to be traced. They can refuse the connection or
    // close the pipe if not.
    TraceControllerPtr controller_ptr;
    connection->ConnectToService(&controller_ptr);
    controller_ptrs_.AddInterfacePtr(controller_ptr.Pass());
    return true;
  }

  // mojo::InterfaceFactory<TraceCoordinator> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<TraceCoordinator> request) override {
    coordinator_bindings_.AddBinding(this, request.Pass());
  }

  // tracing::TraceCoordinator implementation.
  void Start(mojo::ScopedDataPipeProducerHandle stream,
             const mojo::String& categories) override {
    sink_.reset(new TraceDataSink(stream.Pass()));
    controller_ptrs_.ForAllPtrs(
        [categories, this](TraceController* controller) {
          TraceDataCollectorPtr ptr;
          collector_impls_.push_back(
              new CollectorImpl(GetProxy(&ptr), sink_.get()));
          controller->StartTracing(categories, ptr.Pass());
        });
  }
  void StopAndFlush() override {
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

  void AllDataCollected() {
    collector_impls_.clear();
    sink_->Flush();
  }

  scoped_ptr<TraceDataSink> sink_;
  ScopedVector<CollectorImpl> collector_impls_;
  mojo::WeakInterfacePtrSet<TraceController> controller_ptrs_;
  mojo::WeakBindingSet<TraceCoordinator> coordinator_bindings_;

  DISALLOW_COPY_AND_ASSIGN(TracingApp);
};

}  // namespace tracing

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new tracing::TracingApp);
  return runner.Run(shell_handle);
}
