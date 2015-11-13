// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/runner/child/runner_connection.h"
#include "mojo/runner/child/test_native_service.mojom.h"
#include "mojo/runner/init.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

class TargetApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::runner::test::TestNativeService,
      public mojo::InterfaceFactory<mojo::runner::test::TestNativeService> {
 public:
  TargetApplicationDelegate() {}
  ~TargetApplicationDelegate() override {}

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {}
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::runner::test::TestNativeService>(this);
    return true;
  }

  // mojo::runner::test::TestNativeService:
  void Invert(bool from_driver, const InvertCallback& callback) override {
    callback.Run(!from_driver);
  }

  // mojo::InterfaceFactory<mojo::runner::test::TestNativeService>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::runner::test::TestNativeService>
                  request) override {
    bindings_.AddBinding(this, request.Pass());
  }

  mojo::WeakBindingSet<mojo::runner::test::TestNativeService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

class ProcessDelegate : public mojo::embedder::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::runner::InitializeLogging();
  mojo::runner::WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
#endif

  {
    mojo::embedder::Init();

    ProcessDelegate process_delegate;
    base::Thread io_thread("io_thread");
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread.StartWithOptions(io_thread_options));

    mojo::embedder::InitIPCSupport(
        mojo::embedder::ProcessType::NONE, io_thread.task_runner().get(),
        &process_delegate, io_thread.task_runner().get(),
        mojo::embedder::ScopedPlatformHandle());

    mojo::InterfaceRequest<mojo::Application> application_request;
    scoped_ptr<mojo::runner::RunnerConnection> connection(
        mojo::runner::RunnerConnection::ConnectToRunner(&application_request));

    TargetApplicationDelegate delegate;
    {
      base::MessageLoop loop(mojo::common::MessagePumpMojo::Create());
      mojo::ApplicationImpl impl(&delegate, application_request.Pass());
      loop.Run();
    }

    connection.reset();

    mojo::embedder::ShutdownIPCSupport();
  }

  return 0;
}
