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
#include "mojo/runner/child/test_native_main.h"
#include "mojo/runner/init.h"
#include "mojo/shell/application_manager_apptests.mojom.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

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
    mojo::shell::mojom::ApplicationManagerPtr application_manager;
    app_->ConnectToService("mojo:shell", &application_manager);

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
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk"))
      child_command_line.AppendSwitch("use-new-edk");

    mojo::embedder::HandlePassingInformation handle_passing_info;

    // Create the channel to be shared with the target process.
    mojo::embedder::PlatformChannelPair platform_channel_pair;
    // Give one end to the shell so that it can create an instance.
    mojo::embedder::ScopedPlatformHandle platform_channel =
        platform_channel_pair.PassServerHandle();

    mojo::ScopedMessagePipeHandle handle(mojo::embedder::CreateChannel(
        std::move(platform_channel),
        base::Bind(&TargetApplicationDelegate::DidCreateChannel,
                   weak_factory_.GetWeakPtr()),
        base::ThreadTaskRunnerHandle::Get()));

    // The platform handle used for the new EDK's broker.
    mojo::edk::PlatformChannelPair broker_channel_pair;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk")) {
      std::string client_handle_as_string =
          broker_channel_pair.PrepareToPassClientHandleToChildProcessAsString(
              &handle_passing_info);
      MojoResult rv = MojoWriteMessage(
          handle.get().value(), client_handle_as_string.c_str(),
          static_cast<uint32_t>(client_handle_as_string.size()), nullptr, 0,
          MOJO_WRITE_MESSAGE_FLAG_NONE);
      DCHECK_EQ(rv, MOJO_RESULT_OK);
    }

    mojo::CapabilityFilterPtr filter(mojo::CapabilityFilter::New());
    mojo::Array<mojo::String> test_interfaces;
    test_interfaces.push_back(
        mojo::shell::test::mojom::CreateInstanceForHandleTest::Name_);
    filter->filter.insert("mojo:mojo_shell_apptests",
                          std::move(test_interfaces));
    mojo::shell::mojom::PIDReceiverPtr receiver;
    mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
        GetProxy(&receiver);
    application_manager->CreateInstanceForHandle(
        mojo::ScopedHandle(mojo::Handle(handle.release().value())),
        "exe:application_manager_apptest_target", std::move(filter),
        std::move(request));
    // Put the other end on the command line used to launch the target.
    platform_channel_pair.PrepareToPassClientHandleToChildProcess(
        &child_command_line, &handle_passing_info);

    base::LaunchOptions options;
  #if defined(OS_WIN)
    options.handles_to_inherit = &handle_passing_info;
  #elif defined(OS_POSIX)
    options.fds_to_remap = &handle_passing_info;
  #endif
    target_ = base::LaunchProcess(child_command_line, options);
    DCHECK(target_.IsValid());
    receiver->SetPID(target_.Pid());

    if (base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk")) {
      MojoHandle platform_handle_wrapper;
      MojoResult rv = mojo::edk::CreatePlatformHandleWrapper(
          broker_channel_pair.PassServerHandle(), &platform_handle_wrapper);
      DCHECK_EQ(rv, MOJO_RESULT_OK);
      application_manager->RegisterProcessWithBroker(
          target_.Pid(),
          mojo::ScopedHandle(mojo::Handle(platform_handle_wrapper)));
    }
  }
  bool ConfigureIncomingConnection(
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

  void DidCreateChannel(mojo::embedder::ChannelInfo* channel_info) {}

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

  mojo::runner::InitializeLogging();

  TargetApplicationDelegate delegate;
  return mojo::runner::TestNativeMain(&delegate);
}
