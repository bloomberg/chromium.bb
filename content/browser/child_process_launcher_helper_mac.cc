// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "content/browser/bootstrap_sandbox_manager_mac.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/mach_broker_mac.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#include "sandbox/mac/pre_exec_delegate.h"

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

std::unique_ptr<FileDescriptorInfo>
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
  std::unique_ptr<base::FileHandleMappingVector> fds_to_map =
      files_to_register.GetMappingWithIDAdjustment(
          base::GlobalDescriptors::kBaseDescriptor);

  options->environ = delegate_->GetEnvironment();
  // fds_to_remap will de deleted in AfterLaunchOnLauncherThread() below.
  options->fds_to_remap = fds_to_map.release();

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

  const SandboxType sandbox_type = delegate_->GetSandboxType();
  std::unique_ptr<sandbox::PreExecDelegate> pre_exec_delegate;
  if (BootstrapSandboxManager::ShouldEnable()) {
    BootstrapSandboxManager* sandbox_manager =
        BootstrapSandboxManager::GetInstance();
    if (sandbox_manager->EnabledForSandbox(sandbox_type)) {
      pre_exec_delegate = sandbox_manager->sandbox()->NewClient(sandbox_type);
    }
  }
  // options now owns the pre_exec_delegate which will be delete on
  // AfterLaunchOnLauncherThread below.
  options->pre_exec_delegate = pre_exec_delegate.release();
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileDescriptorInfo> files_to_register,
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
  delete options.fds_to_remap;

  std::unique_ptr<sandbox::PreExecDelegate> pre_exec_delegate =
    base::WrapUnique(static_cast<sandbox::PreExecDelegate*>(
        options.pre_exec_delegate));

  MachBroker* broker = MachBroker::GetInstance();
  if (process.process.IsValid()) {
    broker->AddPlaceholderForPid(process.process.Pid(), child_process_id());
  } else {
    if (pre_exec_delegate) {
      BootstrapSandboxManager::GetInstance()->sandbox()->RevokeToken(
          pre_exec_delegate->sandbox_token());
    }
  }

  // After updating the broker, release the lock and let the child's message be
  // processed on the broker's thread.
  broker->GetLock().Release();
}

// static
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

// static
void ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread(
      base::Process process, bool background) {
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(MachBroker::GetInstance(), background);
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
  // No file passing from the manifest on Mac yet.
  DCHECK(required_files.empty());
}

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
