// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

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

namespace {

class EDKState : public mojo::embedder::ProcessDelegate {
 public:
  EDKState() : io_thread_("io_thread") {
    mojo::embedder::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.task_runner().get();
    CHECK(io_runner_.get());

    // TODO(vtl): This should be SLAVE, not NONE.
    mojo::embedder::InitIPCSupport(mojo::embedder::ProcessType::NONE,
                                   io_runner_, this, io_runner_,
                                   mojo::embedder::ScopedPlatformHandle());
  }
  ~EDKState() override { mojo::embedder::ShutdownIPCSupport(); }

 private:
  // mojo::embedder::ProcessDelegate:
  void OnShutdownComplete() override {}

  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  DISALLOW_COPY_AND_ASSIGN(EDKState);
};

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
    EDKState edk;

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
  }

  return 0;
}
