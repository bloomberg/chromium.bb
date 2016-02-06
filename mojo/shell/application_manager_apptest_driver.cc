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
#include "mojo/common/weak_binding_set.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/shell/application_manager_apptests.mojom.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/runner/child/test_native_main.h"
#include "mojo/shell/runner/common/switches.h"
#include "mojo/shell/runner/init.h"

using mojo::shell::test::mojom::CreateInstanceForHandleTestPtr;
using mojo::shell::test::mojom::Driver;

namespace {

class TargetApplicationDelegate : public mojo::ApplicationDelegate,
                                  public mojo::InterfaceFactory<Driver>,
                                  public Driver {
 public:
  TargetApplicationDelegate() : app_(nullptr), weak_factory_(this) {}
  ~TargetApplicationDelegate() override {}

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    app_ = app;

    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_EXE, &target_path));
  #if defined(OS_WIN)
    target_path = target_path.Append(
        FILE_PATH_LITERAL("application_manager_apptest_target.exe"));
  #else
    target_path = target_path.Append(
        FILE_PATH_LITERAL("application_manager_apptest_target"));
  #endif

    base::CommandLine child_command_line(target_path);
    // Forward the wait-for-debugger flag but nothing else - we don't want to
    // stamp on the platform-channel flag.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWaitForDebugger)) {
      child_command_line.AppendSwitch(switches::kWaitForDebugger);
    }

    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk"));
    child_command_line.AppendSwitch("use-new-edk");

    mojo::shell::mojom::PIDReceiverPtr receiver;
    mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
        GetProxy(&receiver);

    // Create the channel to be shared with the target process. Pass one end
    // on the command line.
    mojo::edk::PlatformChannelPair platform_channel_pair;
    mojo::edk::HandlePassingInformation handle_passing_info;
    platform_channel_pair.PrepareToPassClientHandleToChildProcess(
        &child_command_line, &handle_passing_info);

    // Generate a token for the child to find and connect to a primordial pipe
    // and pass that as well.
    std::string primordial_pipe_token = mojo::edk::GenerateRandomToken();
    child_command_line.AppendSwitchASCII(switches::kPrimordialPipeToken,
                                         primordial_pipe_token);

    // Allocate the pipe locally.
    mojo::edk::CreateParentMessagePipe(
        primordial_pipe_token,
        base::Bind(&TargetApplicationDelegate::OnMessagePipeCreated,
                   weak_factory_.GetWeakPtr(),
                   base::ThreadTaskRunnerHandle::Get(),
                   base::Passed(&request)));

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

  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<Driver>(this);
    return true;
  }

  // mojo::InterfaceFactory<Driver>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Driver> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // Driver:
  void QuitDriver() override {
    target_.Terminate(0, false);
    app_->Quit();
  }

  static void OnMessagePipeCreated(
      base::WeakPtr<TargetApplicationDelegate> weak_self,
      scoped_refptr<base::TaskRunner> task_runner,
      mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request,
      mojo::ScopedMessagePipeHandle pipe) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&TargetApplicationDelegate::OnMessagePipeCreatedOnMainThread,
                   weak_self, base::Passed(&request), base::Passed(&pipe)));
  }

  void OnMessagePipeCreatedOnMainThread(
      mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request,
      mojo::ScopedMessagePipeHandle pipe) {
    mojo::CapabilityFilterPtr filter(mojo::CapabilityFilter::New());
    mojo::Array<mojo::String> test_interfaces;
    test_interfaces.push_back(
        mojo::shell::test::mojom::CreateInstanceForHandleTest::Name_);
    filter->filter.insert("mojo:mojo_shell_apptests",
                          std::move(test_interfaces));

    mojo::shell::mojom::ApplicationManagerPtr application_manager;
    app_->ConnectToService("mojo:shell", &application_manager);
    application_manager->CreateInstanceForHandle(
        mojo::ScopedHandle(mojo::Handle(pipe.release().value())),
        "exe:application_manager_apptest_target", std::move(filter),
        std::move(request));
  }

  mojo::ApplicationImpl* app_;
  base::Process target_;
  mojo::WeakBindingSet<Driver> bindings_;
  base::WeakPtrFactory<TargetApplicationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  TargetApplicationDelegate delegate;
  return mojo::shell::TestNativeMain(&delegate);
}
