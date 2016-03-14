// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/common/client_util.h"
#include "mojo/shell/runner/common/switches.h"
#include "mojo/shell/runner/init.h"
#include "mojo/shell/tests/shell/shell_unittest.mojom.h"

namespace {

class Driver : public mojo::ShellClient,
               public mojo::InterfaceFactory<mojo::shell::test::mojom::Driver>,
               public mojo::shell::test::mojom::Driver {
 public:
  Driver() : weak_factory_(this) {}
  ~Driver() override {}

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override {
    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_EXE, &target_path));
  #if defined(OS_WIN)
    target_path = target_path.Append(
        FILE_PATH_LITERAL("shell_unittest_target.exe"));
  #else
    target_path = target_path.Append(
        FILE_PATH_LITERAL("shell_unittest_target"));
  #endif

    base::CommandLine child_command_line(target_path);
    // Forward the wait-for-debugger flag but nothing else - we don't want to
    // stamp on the platform-channel flag.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWaitForDebugger)) {
      child_command_line.AppendSwitch(switches::kWaitForDebugger);
    }

    // Create the channel to be shared with the target process. Pass one end
    // on the command line.
    mojo::edk::PlatformChannelPair platform_channel_pair;
    mojo::edk::HandlePassingInformation handle_passing_info;
    platform_channel_pair.PrepareToPassClientHandleToChildProcess(
        &child_command_line, &handle_passing_info);

    mojo::shell::mojom::ShellClientPtr client =
        mojo::shell::PassShellClientRequestOnCommandLine(&child_command_line);
    mojo::shell::mojom::PIDReceiverPtr receiver;

    mojo::Identity target("exe:shell_unittest_target",
                          mojo::shell::mojom::kInheritUserID);
    mojo::Connector::ConnectParams params(target);
    params.set_client_process_connection(std::move(client),
                                         GetProxy(&receiver));
    scoped_ptr<mojo::Connection> connection = connector->Connect(&params);
    connection->AddConnectionCompletedClosure(
        base::Bind(&Driver::OnConnectionCompleted, base::Unretained(this)));

    base::LaunchOptions options;
  #if defined(OS_WIN)
    options.handles_to_inherit = &handle_passing_info;
  #elif defined(OS_POSIX)
    options.fds_to_remap = &handle_passing_info;
  #endif
    target_ = base::LaunchProcess(child_command_line, options);
    DCHECK(target_.IsValid());
    receiver->SetPID(target_.Pid());
    mojo::edk::ChildProcessLaunched(target_.Handle(),
                                    platform_channel_pair.PassServerHandle());
  }

  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddInterface<mojo::shell::test::mojom::Driver>(this);
    return true;
  }

  // mojo::InterfaceFactory<Driver>:
  void Create(mojo::Connection* connection,
              mojo::shell::test::mojom::DriverRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // Driver:
  void QuitDriver() override {
    target_.Terminate(0, false);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnConnectionCompleted() {}

  base::Process target_;
  mojo::BindingSet<mojo::shell::test::mojom::Driver> bindings_;
  base::WeakPtrFactory<Driver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  Driver driver;
  return mojo::shell::TestNativeMain(&driver);
}
