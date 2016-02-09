// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/runner/child/test_native_main.h"

#include <utility>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/runner/child/runner_connection.h"
#include "mojo/shell/runner/init.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"

namespace mojo {
namespace shell {
namespace {

class ProcessDelegate : public mojo::embedder::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}  // namespace

int TestNativeMain(mojo::ShellClient* shell_client) {
  mojo::shell::WaitForDebuggerIfNecessary();

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

    mojo::embedder::InitIPCSupport(
        mojo::embedder::ProcessType::NONE, &process_delegate,
        io_thread.task_runner().get(), mojo::embedder::ScopedPlatformHandle());

    mojo::ShellClientRequest request;
    scoped_ptr<mojo::shell::RunnerConnection> connection(
        mojo::shell::RunnerConnection::ConnectToRunner(
            &request, ScopedMessagePipeHandle()));
    base::MessageLoop loop(mojo::common::MessagePumpMojo::Create());
    mojo::ShellConnection impl(shell_client, std::move(request));
    loop.Run();

    mojo::embedder::ShutdownIPCSupport();
  }

  return 0;
}

}  // namespace shell
}  // namespace mojo
