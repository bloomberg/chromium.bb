// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mojo_test_connector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/test_launcher.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/catalog/store.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/host/service_process_launcher.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/switches.h"

namespace {

const char kTestRunnerName[] = "mash_browser_tests";

// State created per test to register a client process with the background
// service manager.
class MojoTestState : public content::TestState {
 public:
  explicit MojoTestState(
      service_manager::BackgroundServiceManager* background_service_manager)
      : background_service_manager_(background_service_manager),
        weak_factory_(this) {}
  ~MojoTestState() override {}

  void Init(base::CommandLine* command_line,
            base::TestLauncher::LaunchOptions* test_launch_options) {
    command_line->AppendSwitch(MojoTestConnector::kTestSwitch);
    command_line->AppendSwitch(switches::kChildProcess);
    command_line->AppendSwitchASCII(switches::kMusConfig, switches::kMash);

    platform_channel_ = base::MakeUnique<mojo::edk::PlatformChannelPair>();

    platform_channel_->PrepareToPassClientHandleToChildProcess(
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

    // Create the pipe token, as it must be passed to children processes via the
    // command line.
    service_ = service_manager::PassServiceRequestOnCommandLine(
        &process_connection_, command_line);

    // ChildProcessLaunched may be called on an arbitrary thread, so track the
    // current TaskRunner and post back to it when we want to send the PID.
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

 private:
  // content::TestState:
  void ChildProcessLaunched(base::ProcessHandle handle,
                            base::ProcessId pid) override {
    platform_channel_->ChildProcessLaunched();
    process_connection_.Connect(
        handle,
        mojo::edk::ConnectionParams(platform_channel_->PassServerHandle()));

    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&MojoTestState::SetupService,
                                           weak_factory_.GetWeakPtr(), pid));
  }

  // Called on the main thread only.
  // This registers the services needed for the test. This is not done until
  // after ChildProcessLaunched as previous test runs will tear down existing
  // connections.
  void SetupService(base::ProcessId pid) {
    background_service_manager_->RegisterService(
        service_manager::Identity(content::mojom::kPackagedServicesServiceName,
                                  service_manager::mojom::kRootUserID),
        std::move(service_),
        service_manager::mojom::PIDReceiverRequest(&pid_receiver_));

    DCHECK(pid_receiver_.is_bound());
    pid_receiver_->SetPID(pid);
    pid_receiver_.reset();
  }

  mojo::edk::PendingProcessConnection process_connection_;
  service_manager::BackgroundServiceManager* const background_service_manager_;

  // The ServicePtr must be created before child process launch so that the pipe
  // can be set on the command line. It is held until SetupService is called at
  // which point |background_service_manager_| takes over ownership.
  service_manager::mojom::ServicePtr service_;

  // NOTE: HandlePassingInformation must remain valid through process launch,
  // hence it lives here instead of within Init()'s stack.
  mojo::edk::HandlePassingInformation handle_passing_info_;

  std::unique_ptr<mojo::edk::PlatformChannelPair> platform_channel_;
  service_manager::mojom::PIDReceiverPtr pid_receiver_;
  scoped_refptr<base::TaskRunner> main_task_runner_ = nullptr;

  base::WeakPtrFactory<MojoTestState> weak_factory_;

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

// ServiceProcessLauncher::Delegate that makes exe:mash_browser_tests to
// exe:browser_tests and removes '--run-in-mash'.
class MojoTestConnector::ServiceProcessLauncherDelegateImpl
    : public service_manager::ServiceProcessLauncher::Delegate {
 public:
  ServiceProcessLauncherDelegateImpl() {}
  ~ServiceProcessLauncherDelegateImpl() override {}

 private:
  // service_manager::ServiceProcessLauncherDelegate:
  void AdjustCommandLineArgumentsForTarget(
      const service_manager::Identity& target,
      base::CommandLine* command_line) override {
    if (target.name() != content::mojom::kPackagedServicesServiceName) {
      if (target.name() == kTestRunnerName) {
        RemoveMashFromBrowserTests(command_line);
        command_line->SetProgram(
            base::CommandLine::ForCurrentProcess()->GetProgram());
      }
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

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

// static
const char MojoTestConnector::kTestSwitch[] = "is_test";
// static
const char MojoTestConnector::kMashApp[] = "mash-app";

MojoTestConnector::MojoTestConnector(
    std::unique_ptr<base::Value> catalog_contents)
    : service_process_launcher_delegate_(
          new ServiceProcessLauncherDelegateImpl),
      background_service_manager_(service_process_launcher_delegate_.get(),
                                  std::move(catalog_contents)) {}

service_manager::mojom::ServiceRequest MojoTestConnector::Init() {
  // In single-process test mode, browser code will initialize the EDK and IPC.
  // Otherwise we ensure it's initialized here.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          content::kSingleProcessTestsFlag)) {
    mojo::edk::Init();
    ipc_thread_ = base::MakeUnique<base::Thread>("IPC thread");
    ipc_thread_->StartWithOptions(base::Thread::Options(
        base::MessageLoop::TYPE_IO, 0));
    ipc_support_ = base::MakeUnique<mojo::edk::ScopedIPCSupport>(
        ipc_thread_->task_runner(),
        mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);
  }

  service_manager::mojom::ServicePtr service;
  service_manager::mojom::ServiceRequest request(&service);
  background_service_manager_.RegisterService(
      service_manager::Identity(kTestRunnerName,
                                service_manager::mojom::kRootUserID),
      std::move(service), nullptr);
  return request;
}

MojoTestConnector::~MojoTestConnector() {}

std::unique_ptr<content::TestState> MojoTestConnector::PrepareForTest(
    base::CommandLine* command_line,
    base::TestLauncher::LaunchOptions* test_launch_options) {
  auto test_state =
      base::MakeUnique<MojoTestState>(&background_service_manager_);
  test_state->Init(command_line, test_launch_options);
  return test_state;
}
