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
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/shell.h"
#include "services/shell/switches.h"

using shell::mojom::ShellClient;
using shell::mojom::ShellClientPtr;

namespace {

const char kTestRunnerName[] = "mojo:test-runner";
const char kTestName[] = "exe:chrome";

// Returns the Dictionary value of |parent| under the specified key, creating
// and adding as necessary.
base::DictionaryValue* EnsureDictionary(base::DictionaryValue* parent,
                                        const char* key) {
  base::DictionaryValue* dictionary = nullptr;
  if (parent->GetDictionary(key, &dictionary))
    return dictionary;

  std::unique_ptr<base::DictionaryValue> owned_dictionary(
      new base::DictionaryValue);
  dictionary = owned_dictionary.get();
  parent->Set(key, std::move(owned_dictionary));
  return dictionary;
}

// This builds a permissive catalog with the addition of the 'instance_name'
// permission.
std::unique_ptr<shell::TestCatalogStore> BuildTestCatalogStore() {
  std::unique_ptr<base::ListValue> apps(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> test_app_config =
      shell::BuildPermissiveSerializedAppInfo(kTestRunnerName, "test");
  base::DictionaryValue* capabilities =
      EnsureDictionary(test_app_config.get(), catalog::Store::kCapabilitiesKey);
  base::DictionaryValue* required_capabilities =
      EnsureDictionary(capabilities, catalog::Store::kCapabilities_RequiredKey);
  std::unique_ptr<base::ListValue> required_shell_classes(new base::ListValue);
  required_shell_classes->AppendString("instance_name");
  required_shell_classes->AppendString("client_process");
  std::unique_ptr<base::DictionaryValue> shell_caps(new base::DictionaryValue);
  shell_caps->Set(catalog::Store::kCapabilities_ClassesKey,
                  std::move(required_shell_classes));
  required_capabilities->Set("mojo:shell", std::move(shell_caps));
  apps->Append(std::move(test_app_config));
  return base::WrapUnique(new shell::TestCatalogStore(std::move(apps)));
}

// BackgroundTestState maintains all the state necessary to bind the test to
// mojo. This class is only used on the thread created by BackgroundShell.
class BackgroundTestState {
 public:
  BackgroundTestState() {}
  ~BackgroundTestState() {}

  // Prepares the command line and other setup for connecting the test to mojo.
  // Must be paired with a clal to ChildProcessLaunched().
  void Connect(base::CommandLine* command_line,
               shell::Shell* shell,
               const std::string& instance,
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
    shell::mojom::ShellClientPtr client =
        shell::PassShellClientRequestOnCommandLine(command_line);

    std::unique_ptr<shell::ConnectParams> params(new shell::ConnectParams);
    params->set_source(shell::CreateShellIdentity());
    params->set_target(
        shell::Identity(kTestName, shell::mojom::kRootUserID, instance));

    shell::mojom::ClientProcessConnectionPtr client_process_connection =
        shell::mojom::ClientProcessConnection::New();
    client_process_connection->shell_client =
        client.PassInterface().PassHandle();
    client_process_connection->pid_receiver_request =
        mojo::GetProxy(&pid_receiver_).PassMessagePipe();
    params->set_client_process_connection(std::move(client_process_connection));
    shell->Connect(std::move(params));
  }

  // Called after the test process has launched. Completes the registration done
  // in Connect().
  void ChildProcessLaunched(base::ProcessHandle handle, base::ProcessId pid) {
    pid_receiver_->SetPID(pid);
    mojo_ipc_channel_->ChildProcessLaunched();
    mojo::edk::ChildProcessLaunched(
        handle, mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(
                    mojo_ipc_channel_->PassServerHandle().release().handle)));
  }

 private:
  // Used to back the NodeChannel between the parent and child node.
  std::unique_ptr<mojo::edk::PlatformChannelPair> mojo_ipc_channel_;

  mojo::edk::HandlePassingInformation handle_passing_info_;

  shell::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTestState);
};

// Called used destroy BackgroundTestState on the background thread.
void DestroyBackgroundStateOnBackgroundThread(
    std::unique_ptr<BackgroundTestState> state,
    shell::Shell* shell) {}

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
    background_shell_->ExecuteOnShellThread(
        base::Bind(&DestroyBackgroundStateOnBackgroundThread,
                   base::Passed(&background_state_)));
  }

  void Init(base::CommandLine* command_line,
            base::TestLauncher::LaunchOptions* test_launch_options) {
    base::WaitableEvent signal(true, false);
    background_shell_->ExecuteOnShellThread(base::Bind(
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
    base::WaitableEvent signal(true, false);
    background_shell_->ExecuteOnShellThread(
        base::Bind(&MojoTestState::ChildProcessLaunchedOnBackgroundThread,
                   base::Unretained(this), handle, pid, &signal));
    signal.Wait();
  }

  void ChildProcessLaunchedOnBackgroundThread(base::ProcessHandle handle,
                                              base::ProcessId pid,
                                              base::WaitableEvent* signal,
                                              shell::Shell* shell) {
    background_state_->ChildProcessLaunched(handle, pid);
    signal->Signal();
  }

  void BindOnBackgroundThread(
      base::WaitableEvent* signal,
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options,
      shell::Shell* shell) {
    static int instance_id = 0;
    const std::string instance_name =
        "instance-" + base::IntToString(instance_id++);
    background_state_.reset(new BackgroundTestState);
    background_state_->Connect(command_line, shell, instance_name,
                               test_launch_options);
    signal->Signal();
  }

  shell::BackgroundShell* background_shell_;
  std::unique_ptr<BackgroundTestState> background_state_;

  DISALLOW_COPY_AND_ASSIGN(MojoTestState);
};

}  // namespace

// static
const char MojoTestConnector::kTestSwitch[] = "is_test";

MojoTestConnector::MojoTestConnector() {}

shell::mojom::ShellClientRequest MojoTestConnector::Init() {
  std::unique_ptr<shell::BackgroundShell::InitParams> init_params(
      new shell::BackgroundShell::InitParams);
  init_params->catalog_store = BuildTestCatalogStore();
  // When running in single_process mode chrome initializes the edk.
  init_params->init_edk = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      content::kSingleProcessTestsFlag);
  background_shell_.Init(std::move(init_params));
  return background_shell_.CreateShellClientRequest(kTestRunnerName);
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
