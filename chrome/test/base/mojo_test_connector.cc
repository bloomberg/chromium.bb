// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mojo_test_connector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_launcher.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/catalog/store.h"
#include "services/shell/background/tests/test_catalog_store.h"
#include "services/shell/native_runner_delegate.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/service_manager.h"
#include "services/shell/switches.h"

using shell::mojom::Service;
using shell::mojom::ServicePtr;

namespace {

const char kTestRunnerName[] = "exe:mash_browser_tests";
const char kTestName[] = "exe:chrome";

// BackgroundTestState maintains all the state necessary to bind the test to
// mojo. This class is only used on the thread created by BackgroundShell.
class BackgroundTestState {
 public:
  BackgroundTestState() : child_token_(mojo::edk::GenerateRandomToken()) {}
  ~BackgroundTestState() {}

  // Prepares the command line and other setup for connecting the test to mojo.
  // Must be paired with a call to ChildProcessLaunched().
  void Connect(base::CommandLine* command_line,
               shell::ServiceManager* service_manager,
               base::TestLauncher::LaunchOptions* test_launch_options) {
    command_line->AppendSwitch(MojoTestConnector::kTestSwitch);
    command_line->AppendSwitch(switches::kChildProcess);
    mojo_ipc_channel_.reset(new mojo::edk::PlatformChannelPair);
    mojo_ipc_channel_->PrepareToPassClientHandleToChildProcess(
        command_line, &handle_passing_info_);
#if defined(OS_WIN)
    test_launch_options->inherit_handles = true;
    test_launch_options->handles_to_inherit = &handle_passing_info_;
#if defined(OFFICIAL_BUILD)
    CHECK(false) << "Launching mojo process with inherit_handles is insecure!";
#endif
#elif defined(OS_POSIX)
    test_launch_options->fds_to_remap = &handle_passing_info_;
#else
#error "Unsupported"
#endif
    shell::mojom::ServicePtr service =
        shell::PassServiceRequestOnCommandLine(command_line, child_token_);

    std::unique_ptr<shell::ConnectParams> params(new shell::ConnectParams);
    params->set_source(shell::CreateServiceManagerIdentity());
    // Use the default instance name (which should be "browser"). Otherwise a
    // service (e.g. ash) that connects to the default "exe:chrome" will spawn
    // a new instance.
    params->set_target(shell::Identity(kTestName, shell::mojom::kRootUserID));

    shell::mojom::ClientProcessConnectionPtr client_process_connection =
        shell::mojom::ClientProcessConnection::New();
    client_process_connection->service =
        service.PassInterface().PassHandle();
    client_process_connection->pid_receiver_request =
        mojo::GetProxy(&pid_receiver_).PassMessagePipe();
    params->set_client_process_connection(std::move(client_process_connection));
    service_manager->Connect(std::move(params));
  }

  // Called after the test process has launched. Completes the registration done
  // in Connect().
  void ChildProcessLaunched(base::ProcessHandle handle, base::ProcessId pid) {
    pid_receiver_->SetPID(pid);
    mojo_ipc_channel_->ChildProcessLaunched();
    mojo::edk::ChildProcessLaunched(
        handle, mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(
                    mojo_ipc_channel_->PassServerHandle().release().handle)),
        child_token_);
  }

 private:
  // Used to back the NodeChannel between the parent and child node.
  const std::string child_token_;
  std::unique_ptr<mojo::edk::PlatformChannelPair> mojo_ipc_channel_;

  mojo::edk::HandlePassingInformation handle_passing_info_;

  shell::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTestState);
};

// Called used destroy BackgroundTestState on the background thread.
void DestroyBackgroundStateOnBackgroundThread(
    std::unique_ptr<BackgroundTestState> state,
    shell::ServiceManager* service_manager) {}

// State created per test. Manages creation of the corresponding
// BackgroundTestState and making sure processing runs on the right threads.
class MojoTestState : public content::TestState {
 public:
  explicit MojoTestState(shell::BackgroundShell* background_shell)
      : background_shell_(background_shell) {}

  ~MojoTestState() override {
    DCHECK(background_state_);
    // BackgroundState needs to be destroyed on the background thread. We're
    // guaranteed |background_shell_| has been created by the time we reach
    // here as Init() blocks until |background_shell_| has been created.
    background_shell_->ExecuteOnServiceManagerThread(
        base::Bind(&DestroyBackgroundStateOnBackgroundThread,
                   base::Passed(&background_state_)));
  }

  void Init(base::CommandLine* command_line,
            base::TestLauncher::LaunchOptions* test_launch_options) {
    base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    background_shell_->ExecuteOnServiceManagerThread(base::Bind(
        &MojoTestState::BindOnBackgroundThread, base::Unretained(this), &signal,
        command_line, test_launch_options));
    signal.Wait();
  }

 private:
  // content::TestState:
  void ChildProcessLaunched(base::ProcessHandle handle,
                            base::ProcessId pid) override {
    // This is called on a random thread. We need to ensure BackgroundTestState
    // is only called on the background thread, and we wait for
    // ChildProcessLaunchedOnBackgroundThread() to be run before continuing so
    // that |handle| is still valid.
    base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    background_shell_->ExecuteOnServiceManagerThread(
        base::Bind(&MojoTestState::ChildProcessLaunchedOnBackgroundThread,
                   base::Unretained(this), handle, pid, &signal));
    signal.Wait();
  }

  void ChildProcessLaunchedOnBackgroundThread(
      base::ProcessHandle handle,
      base::ProcessId pid,
      base::WaitableEvent* signal,
      shell::ServiceManager* service_manager) {
    background_state_->ChildProcessLaunched(handle, pid);
    signal->Signal();
  }

  void BindOnBackgroundThread(
      base::WaitableEvent* signal,
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options,
      shell::ServiceManager* service_manager) {
    background_state_.reset(new BackgroundTestState);
    background_state_->Connect(command_line, service_manager,
                               test_launch_options);
    signal->Signal();
  }

  shell::BackgroundShell* background_shell_;
  std::unique_ptr<BackgroundTestState> background_state_;

  DISALLOW_COPY_AND_ASSIGN(MojoTestState);
};

// The name in the manifest results in getting exe:mash_browser_tests used,
// remap that to browser_tests.
void RemoveMashFromBrowserTests(base::CommandLine* command_line) {
  base::FilePath exe_path(command_line->GetProgram());
#if defined(OS_WIN)
  exe_path = exe_path.DirName().Append(FILE_PATH_LITERAL("browser_tests.exe"));
#else
  exe_path = exe_path.DirName().Append(FILE_PATH_LITERAL("browser_tests"));
#endif
  command_line->SetProgram(exe_path);
}

}  // namespace

// NativeRunnerDelegate that makes exe:mash_browser_tests to exe:browser_tests,
// and removes '--run-in-mash'.
class MojoTestConnector::NativeRunnerDelegateImpl
    : public shell::NativeRunnerDelegate {
 public:
  NativeRunnerDelegateImpl() {}
  ~NativeRunnerDelegateImpl() override {}

 private:
  // shell::NativeRunnerDelegate:
  void AdjustCommandLineArgumentsForTarget(
      const shell::Identity& target,
      base::CommandLine* command_line) override {
    if (target.name() != "exe:chrome") {
      if (target.name() == "exe:mash_browser_tests")
        RemoveMashFromBrowserTests(command_line);
      command_line->AppendSwitch(MojoTestConnector::kMashApp);
      return;
    }

    base::CommandLine::StringVector argv(command_line->argv());
    auto iter =
        std::find(argv.begin(), argv.end(), FILE_PATH_LITERAL("--run-in-mash"));
    if (iter != argv.end())
      argv.erase(iter);
    *command_line = base::CommandLine(argv);
  }

  DISALLOW_COPY_AND_ASSIGN(NativeRunnerDelegateImpl);
};

// static
const char MojoTestConnector::kTestSwitch[] = "is_test";
// static
const char MojoTestConnector::kMashApp[] = "mash-app";

MojoTestConnector::MojoTestConnector() {}

shell::mojom::ServiceRequest MojoTestConnector::Init() {
  native_runner_delegate_ = base::MakeUnique<NativeRunnerDelegateImpl>();

  std::unique_ptr<shell::BackgroundShell::InitParams> init_params(
      new shell::BackgroundShell::InitParams);
  // When running in single_process mode chrome initializes the edk.
  init_params->init_edk = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      content::kSingleProcessTestsFlag);
  init_params->native_runner_delegate = native_runner_delegate_.get();
  background_shell_.Init(std::move(init_params));
  return background_shell_.CreateServiceRequest(kTestRunnerName);
}

MojoTestConnector::~MojoTestConnector() {}

std::unique_ptr<content::TestState> MojoTestConnector::PrepareForTest(
    base::CommandLine* command_line,
    base::TestLauncher::LaunchOptions* test_launch_options) {
  std::unique_ptr<MojoTestState> test_state(
      new MojoTestState(&background_shell_));
  test_state->Init(command_line, test_launch_options);
  return std::move(test_state);
}
