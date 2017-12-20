// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/mach_broker_mac.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/mac/common_v2.sb.h"
#include "services/service_manager/sandbox/mac/ppapi_v2.sb.h"
#include "services/service_manager/sandbox/mac/renderer_v2.sb.h"
#include "services/service_manager/sandbox/mac/utility.sb.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace content {
namespace internal {

mojo::edk::ScopedPlatformHandle
ChildProcessLauncherHelper::PrepareMojoPipeHandlesOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  return mojo::edk::ScopedPlatformHandle();
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  return CreateDefaultPosixFilesToMap(
      child_process_id(), mojo_client_handle(),
      false /* include_service_required_files */, GetProcessType(),
      command_line());
}

void ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector.
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);

  options->environ = delegate_->GetEnvironment();

  auto sandbox_type =
      service_manager::SandboxTypeFromCommandLine(*command_line_);

  bool no_sandbox = command_line_->HasSwitch(switches::kNoSandbox) ||
                    service_manager::IsUnsandboxedSandboxType(sandbox_type);

  bool v2_process = sandbox_type == service_manager::SANDBOX_TYPE_PPAPI ||
                    sandbox_type == service_manager::SANDBOX_TYPE_RENDERER ||
                    sandbox_type == service_manager::SANDBOX_TYPE_UTILITY;

  bool use_v2 =
      v2_process && base::FeatureList::IsEnabled(features::kMacV2Sandbox);

  if (use_v2 && !no_sandbox) {
    // Generate the profile string.
    std::string profile =
        std::string(service_manager::kSeatbeltPolicyString_common_v2);

    if (sandbox_type == service_manager::SANDBOX_TYPE_PPAPI) {
      profile += service_manager::kSeatbeltPolicyString_ppapi_v2;
    } else if (sandbox_type == service_manager::SANDBOX_TYPE_RENDERER) {
      profile += service_manager::kSeatbeltPolicyString_renderer_v2;
    } else if (sandbox_type == service_manager::SANDBOX_TYPE_UTILITY) {
      profile += service_manager::kSeatbeltPolicyString_utility;
    }

    // Disable os logging to com.apple.diagnosticd which is a performance
    // problem.
    options->environ.insert(std::make_pair("OS_ACTIVITY_MODE", "disable"));

    seatbelt_exec_client_ = std::make_unique<sandbox::SeatbeltExecClient>();
    seatbelt_exec_client_->SetProfile(profile);

    if (sandbox_type == service_manager::SANDBOX_TYPE_RENDERER ||
        sandbox_type == service_manager::SANDBOX_TYPE_PPAPI) {
      SetupCommonSandboxParameters(seatbelt_exec_client_.get());
    } else if (sandbox_type == service_manager::SANDBOX_TYPE_UTILITY) {
      SetupUtilitySandboxParameters(seatbelt_exec_client_.get(),
                                    *command_line_.get());
    }

    int pipe = seatbelt_exec_client_->SendProfileAndGetFD();

    base::FilePath helper_executable;
    CHECK(PathService::Get(content::CHILD_PROCESS_EXE, &helper_executable));

    options->fds_to_remap.push_back(std::make_pair(pipe, pipe));

    // Update the command line to enable the V2 sandbox and pass the
    // communication FD to the helper executable.
    command_line_->AppendSwitch(switches::kEnableV2Sandbox);
    command_line_->AppendArg("--fd_mapping=" + std::to_string(pipe));
  }

  // Hold the MachBroker lock for the duration of LaunchProcess. The child will
  // send its task port to the parent almost immediately after startup. The Mach
  // message will be delivered to the parent, but updating the record of the
  // launch will wait until after the placeholder PID is inserted below. This
  // ensures that while the child process may send its port to the parent prior
  // to the parent leaving LaunchProcess, the order in which the record in
  // MachBroker is updated is correct.
  MachBroker* broker = MachBroker::GetInstance();
  broker->GetLock().Acquire();

  // Make sure the MachBroker is running, and inform it to expect a check-in
  // from the new process.
  broker->EnsureRunning();
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;
  ChildProcessLauncherHelper::Process process;
  process.process = base::LaunchProcess(*command_line(), options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
  MachBroker* broker = MachBroker::GetInstance();
  if (process.process.IsValid()) {
    broker->AddPlaceholderForPid(process.process.Pid(), child_process_id());
  }

  // After updating the broker, release the lock and let the child's message be
  // processed on the broker's thread.
  broker->GetLock().Release();
}

base::TerminationStatus ChildProcessLauncherHelper::GetTerminationStatus(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead,
    int* exit_code) {
  return known_dead
      ? base::GetKnownDeadTerminationStatus(process.process.Handle(), exit_code)
      : base::GetTerminationStatus(process.process.Handle(), exit_code);
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(
    const base::Process& process, int exit_code, bool wait) {
  return process.Terminate(exit_code, wait);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  base::EnsureProcessTerminated(std::move(process.process));
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  if (process.CanBackgroundProcesses()) {
    process.SetProcessBackgrounded(MachBroker::GetInstance(),
                                   priority.background);
  }
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
  // No file passing from the manifest on Mac yet.
  DCHECK(required_files.empty());
}

// static
void ChildProcessLauncherHelper::ResetRegisteredFilesForTesting() {}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  // Not used yet (until required files are described in the service manifest on
  // Mac).
  NOTREACHED();
  return base::File();
}

}  //  namespace internal
}  // namespace content
