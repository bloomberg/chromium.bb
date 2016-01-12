// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mash/example/window_type_launcher/window_type_launcher.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/runner/child/runner_connection.h"
#include "mojo/runner/init.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/interfaces/application.mojom.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"

namespace {

class ProcessDelegate : public mojo::embedder::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}

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
    mojo::embedder::PreInitializeChildProcess();
    mojo::embedder::Init();

    ProcessDelegate process_delegate;
    base::Thread io_thread("io_thread");
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread.StartWithOptions(io_thread_options));

    mojo::embedder::InitIPCSupport(mojo::embedder::ProcessType::NONE,
                                   &process_delegate,
                                   io_thread.task_runner().get(),
                                   mojo::embedder::ScopedPlatformHandle());

    mojo::InterfaceRequest<mojo::Application> application_request;
    scoped_ptr<mojo::runner::RunnerConnection> connection(
        mojo::runner::RunnerConnection::ConnectToRunner(
            &application_request, mojo::ScopedMessagePipeHandle()));
    base::MessageLoop loop(mojo::common::MessagePumpMojo::Create());
    WindowTypeLauncher delegate;
    mojo::ApplicationImpl impl(&delegate, std::move(application_request));
    loop.Run();

    mojo::embedder::ShutdownIPCSupport();
  }

  return 0;
}
