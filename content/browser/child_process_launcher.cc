// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

#if defined(OS_WIN)
#include "base/files/file_path.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/sandbox_init.h"
#elif defined(OS_MACOSX)
#include "content/browser/bootstrap_sandbox_manager_mac.h"
#include "content/browser/mach_broker_mac.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#include "sandbox/mac/pre_exec_delegate.h"
#elif defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/child_process_launcher_android.h"
#elif defined(OS_POSIX)
#include "base/memory/singleton.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/browser/file_descriptor_info_impl.h"
#include "gin/v8_initializer.h"
#endif

namespace content {

namespace {

typedef base::Callback<void(bool,
#if defined(OS_ANDROID)
                            base::ScopedFD,
#endif
                            base::Process)> NotifyCallback;

void RecordHistogramsOnLauncherThread(base::TimeDelta launch_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  // Log the launch time, separating out the first one (which will likely be
  // slower due to the rest of the browser initializing at the same time).
  static bool done_first_launch = false;
  if (done_first_launch) {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchSubsequent", launch_time);
  } else {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchFirst", launch_time);
    done_first_launch = true;
  }
}

#if defined(OS_ANDROID)
// TODO(sievers): Remove this by defining better what happens on what
// thread in the corresponding Java code.
void OnChildProcessStartedAndroid(const NotifyCallback& callback,
                                  BrowserThread::ID client_thread_id,
                                  const base::TimeTicks begin_launch_time,
                                  base::ScopedFD ipcfd,
                                  base::ProcessHandle handle) {
  // This can be called on the launcher thread or UI thread.
  base::TimeDelta launch_time = base::TimeTicks::Now() - begin_launch_time;
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&RecordHistogramsOnLauncherThread, launch_time));

  base::Closure callback_on_client_thread(
      base::Bind(callback, false, base::Passed(&ipcfd),
                 base::Passed(base::Process(handle))));
  if (BrowserThread::CurrentlyOn(client_thread_id)) {
    callback_on_client_thread.Run();
  } else {
    BrowserThread::PostTask(
        client_thread_id, FROM_HERE, callback_on_client_thread);
  }
}
#endif

void LaunchOnLauncherThread(const NotifyCallback& callback,
                            BrowserThread::ID client_thread_id,
                            int child_process_id,
                            SandboxedProcessLauncherDelegate* delegate,
#if defined(OS_ANDROID)
                            base::ScopedFD ipcfd,
#endif
                            base::CommandLine* cmd_line) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  scoped_ptr<SandboxedProcessLauncherDelegate> delegate_deleter(delegate);
#if defined(OS_WIN)
  bool use_zygote = false;
  bool launch_elevated = delegate->ShouldLaunchElevated();
#elif defined(OS_MACOSX)
  bool use_zygote = false;
  base::EnvironmentMap env = delegate->GetEnvironment();
  base::ScopedFD ipcfd = delegate->TakeIpcFd();
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  bool use_zygote = delegate->ShouldUseZygote();
  base::EnvironmentMap env = delegate->GetEnvironment();
  base::ScopedFD ipcfd = delegate->TakeIpcFd();
#endif
  scoped_ptr<base::CommandLine> cmd_line_deleter(cmd_line);
  base::TimeTicks begin_launch_time = base::TimeTicks::Now();

  base::Process process;
#if defined(OS_WIN)
  if (launch_elevated) {
    base::LaunchOptions options;
    options.start_hidden = true;
    process = base::LaunchElevatedProcess(*cmd_line, options);
  } else {
    process = StartSandboxedProcess(delegate, cmd_line);
  }
#elif defined(OS_POSIX)
  std::string process_type =
      cmd_line->GetSwitchValueASCII(switches::kProcessType);
  scoped_ptr<FileDescriptorInfo> files_to_register(
      FileDescriptorInfoImpl::Create());

#if defined(OS_ANDROID)
  files_to_register->Share(kPrimaryIPCChannel, ipcfd.get());
#else
  files_to_register->Transfer(kPrimaryIPCChannel, std::move(ipcfd));
#endif
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  std::map<int, base::MemoryMappedFile::Region> regions;
  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      *cmd_line, child_process_id, files_to_register.get()
#if defined(OS_ANDROID)
      , &regions
#endif
      );
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  base::PlatformFile natives_pf =
      gin::V8Initializer::GetOpenNativesFileForChildProcesses(
          &regions[kV8NativesDataDescriptor]);
  DCHECK_GE(natives_pf, 0);
  files_to_register->Share(kV8NativesDataDescriptor, natives_pf);

  base::MemoryMappedFile::Region snapshot_region;
  base::PlatformFile snapshot_pf =
      gin::V8Initializer::GetOpenSnapshotFileForChildProcesses(
          &snapshot_region);
  // Failure to load the V8 snapshot is not necessarily an error. V8 can start
  // up (slower) without the snapshot.
  if (snapshot_pf != -1) {
    files_to_register->Share(kV8SnapshotDataDescriptor, snapshot_pf);
    regions.insert(std::make_pair(kV8SnapshotDataDescriptor, snapshot_region));
  }

  if (process_type != switches::kZygoteProcess) {
    cmd_line->AppendSwitch(::switches::kV8NativesPassedByFD);
    if (snapshot_pf != -1) {
      cmd_line->AppendSwitch(::switches::kV8SnapshotPassedByFD);
    }
  }
#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_ANDROID)
  files_to_register->Share(
      kAndroidICUDataDescriptor,
      base::i18n::GetIcuDataFileHandle(&regions[kAndroidICUDataDescriptor]));

  // Android WebView runs in single process, ensure that we never get here
  // when running in single process mode.
  CHECK(!cmd_line->HasSwitch(switches::kSingleProcess));

  StartChildProcess(
      cmd_line->argv(), child_process_id, std::move(files_to_register), regions,
      base::Bind(&OnChildProcessStartedAndroid, callback, client_thread_id,
                 begin_launch_time, base::Passed(&ipcfd)));

#elif defined(OS_POSIX)
  // We need to close the client end of the IPC channel to reliably detect
  // child termination.

#if !defined(OS_MACOSX)
  if (use_zygote) {
    base::ProcessHandle handle = ZygoteHostImpl::GetInstance()->ForkRequest(
        cmd_line->argv(), std::move(files_to_register), process_type);
    process = base::Process(handle);
  } else
  // Fall through to the normal posix case below when we're not zygoting.
#endif  // !defined(OS_MACOSX)
  {
    // Convert FD mapping to FileHandleMappingVector
    base::FileHandleMappingVector fds_to_map =
        files_to_register->GetMappingWithIDAdjustment(
            base::GlobalDescriptors::kBaseDescriptor);

#if !defined(OS_MACOSX)
    if (process_type == switches::kRendererProcess) {
      const int sandbox_fd =
          RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
      fds_to_map.push_back(std::make_pair(
          sandbox_fd,
          GetSandboxFD()));
    }
#endif  // defined(OS_MACOSX)

    // Actually launch the app.
    base::LaunchOptions options;
    options.environ = env;
    options.fds_to_remap = &fds_to_map;

#if defined(OS_MACOSX)
    // Hold the MachBroker lock for the duration of LaunchProcess. The child
    // will send its task port to the parent almost immediately after startup.
    // The Mach message will be delivered to the parent, but updating the
    // record of the launch will wait until after the placeholder PID is
    // inserted below. This ensures that while the child process may send its
    // port to the parent prior to the parent leaving LaunchProcess, the
    // order in which the record in MachBroker is updated is correct.
    MachBroker* broker = MachBroker::GetInstance();
    broker->GetLock().Acquire();

    // Make sure the MachBroker is running, and inform it to expect a
    // check-in from the new process.
    broker->EnsureRunning();

    const SandboxType sandbox_type = delegate->GetSandboxType();
    scoped_ptr<sandbox::PreExecDelegate> pre_exec_delegate;
    if (BootstrapSandboxManager::ShouldEnable()) {
      BootstrapSandboxManager* sandbox_manager =
          BootstrapSandboxManager::GetInstance();
      if (sandbox_manager->EnabledForSandbox(sandbox_type)) {
        pre_exec_delegate = sandbox_manager->sandbox()->NewClient(sandbox_type);
      }
    }
    options.pre_exec_delegate = pre_exec_delegate.get();
#endif  // defined(OS_MACOSX)

    process = base::LaunchProcess(*cmd_line, options);

#if defined(OS_MACOSX)
    if (process.IsValid()) {
      broker->AddPlaceholderForPid(process.Pid(), child_process_id);
    } else {
      if (pre_exec_delegate) {
        BootstrapSandboxManager::GetInstance()->sandbox()->RevokeToken(
            pre_exec_delegate->sandbox_token());
      }
    }

    // After updating the broker, release the lock and let the child's
    // messasge be processed on the broker's thread.
    broker->GetLock().Release();
#endif  // defined(OS_MACOSX)
  }
#endif  // else defined(OS_POSIX)
#if !defined(OS_ANDROID)
  if (process.IsValid()) {
    RecordHistogramsOnLauncherThread(base::TimeTicks::Now() -
                                     begin_launch_time);
  }
  BrowserThread::PostTask(client_thread_id, FROM_HERE,
                          base::Bind(callback,
                                     use_zygote,
                                     base::Passed(&process)));
#endif  // !defined(OS_ANDROID)
}

void TerminateOnLauncherThread(bool zygote, base::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
#if defined(OS_ANDROID)
  VLOG(1) << "ChromeProcess: Stopping process with handle "
          << process.Handle();
  StopChildProcess(process.Handle());
#else
  // Client has gone away, so just kill the process.  Using exit code 0
  // means that UMA won't treat this as a crash.
  process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
#if !defined(OS_MACOSX)
  if (zygote) {
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    ZygoteHostImpl::GetInstance()->EnsureProcessTerminated(process.Handle());
  } else
#endif  // !OS_MACOSX
    base::EnsureProcessTerminated(std::move(process));
#endif  // OS_POSIX
#endif  // defined(OS_ANDROID)
}

void SetProcessBackgroundedOnLauncherThread(base::Process process,
                                            bool background) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  if (process.CanBackgroundProcesses()) {
    process.SetProcessBackgrounded(background);
  }
#if defined(OS_ANDROID)
  SetChildProcessInForeground(process.Handle(), !background);
#endif
}

}  // namespace

ChildProcessLauncher::ChildProcessLauncher(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    int child_process_id,
    Client* client,
    bool terminate_on_shutdown)
    : client_(client),
      termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
      exit_code_(RESULT_CODE_NORMAL_EXIT),
      zygote_(false),
      starting_(true),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
      terminate_child_on_shutdown_(false),
#else
      terminate_child_on_shutdown_(terminate_on_shutdown),
#endif
      weak_factory_(this) {
  DCHECK(CalledOnValidThread());
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));
  Launch(delegate, cmd_line, child_process_id);
}

ChildProcessLauncher::~ChildProcessLauncher() {
  DCHECK(CalledOnValidThread());
  if (process_.IsValid() && terminate_child_on_shutdown_) {
    // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
    // don't this on the UI/IO threads.
    BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
                            base::Bind(&TerminateOnLauncherThread, zygote_,
                                       base::Passed(&process_)));
  }
}

void ChildProcessLauncher::Launch(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    int child_process_id) {
  DCHECK(CalledOnValidThread());

#if defined(OS_ANDROID)
  // Android only supports renderer, sandboxed utility and gpu.
  std::string process_type =
      cmd_line->GetSwitchValueASCII(switches::kProcessType);
  CHECK(process_type == switches::kGpuProcess ||
        process_type == switches::kRendererProcess ||
#if defined(ENABLE_PLUGINS)
        process_type == switches::kPpapiPluginProcess ||
#endif
        process_type == switches::kUtilityProcess)
      << "Unsupported process type: " << process_type;

  // Non-sandboxed utility or renderer process are currently not supported.
  DCHECK(process_type == switches::kGpuProcess ||
         !cmd_line->HasSwitch(switches::kNoSandbox));

  // We need to close the client end of the IPC channel to reliably detect
  // child termination. We will close this fd after we create the child
  // process which is asynchronous on Android.
  base::ScopedFD ipcfd(delegate->TakeIpcFd().release());
#endif
  NotifyCallback reply_callback(base::Bind(&ChildProcessLauncher::DidLaunch,
                                           weak_factory_.GetWeakPtr(),
                                           terminate_child_on_shutdown_));
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&LaunchOnLauncherThread, reply_callback, client_thread_id_,
                 child_process_id, delegate,
#if defined(OS_ANDROID)
                 base::Passed(&ipcfd),
#endif
                 cmd_line));
}

void ChildProcessLauncher::UpdateTerminationStatus(bool known_dead) {
  DCHECK(CalledOnValidThread());
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  if (zygote_) {
    termination_status_ = ZygoteHostImpl::GetInstance()->
        GetTerminationStatus(process_.Handle(), known_dead, &exit_code_);
  } else if (known_dead) {
    termination_status_ =
        base::GetKnownDeadTerminationStatus(process_.Handle(), &exit_code_);
  } else {
#elif defined(OS_MACOSX)
  if (known_dead) {
    termination_status_ =
        base::GetKnownDeadTerminationStatus(process_.Handle(), &exit_code_);
  } else {
#elif defined(OS_ANDROID)
  if (IsChildProcessOomProtected(process_.Handle())) {
    termination_status_ = base::TERMINATION_STATUS_OOM_PROTECTED;
  } else {
#else
  {
#endif
    termination_status_ =
      base::GetTerminationStatus(process_.Handle(), &exit_code_);
  }
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  DCHECK(CalledOnValidThread());
  base::Process to_pass = process_.Duplicate();
  BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
                          base::Bind(&SetProcessBackgroundedOnLauncherThread,
                                     base::Passed(&to_pass), background));
}

void ChildProcessLauncher::DidLaunch(
    base::WeakPtr<ChildProcessLauncher> instance,
    bool terminate_on_shutdown,
    bool zygote,
#if defined(OS_ANDROID)
    base::ScopedFD ipcfd,
#endif
    base::Process process) {
  if (!process.IsValid())
    LOG(ERROR) << "Failed to launch child process";

  if (instance.get()) {
    instance->Notify(zygote,
#if defined(OS_ANDROID)
                     std::move(ipcfd),
#endif
                     std::move(process));
  } else {
    if (process.IsValid() && terminate_on_shutdown) {
      // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
      // don't this on the UI/IO threads.
      BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
                              base::Bind(&TerminateOnLauncherThread, zygote,
                                         base::Passed(&process)));
    }
  }
}

void ChildProcessLauncher::Notify(
    bool zygote,
#if defined(OS_ANDROID)
    base::ScopedFD ipcfd,
#endif
    base::Process process) {
  DCHECK(CalledOnValidThread());
  starting_ = false;
  process_ = std::move(process);

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  zygote_ = zygote;
#endif
  if (process_.IsValid()) {
    client_->OnProcessLaunched();
  } else {
    termination_status_ = base::TERMINATION_STATUS_LAUNCH_FAILED;
    client_->OnProcessLaunchFailed();
  }
}

bool ChildProcessLauncher::IsStarting() {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK(CalledOnValidThread());
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK(CalledOnValidThread());
  return process_;
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    bool known_dead,
    int* exit_code) {
  DCHECK(CalledOnValidThread());
  if (!process_.IsValid()) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = exit_code_;
    return termination_status_;
  }

  UpdateTerminationStatus(known_dead);
  if (exit_code)
    *exit_code = exit_code_;

  // POSIX: If the process crashed, then the kernel closed the socket
  // for it and so the child has already died by the time we get
  // here. Since GetTerminationStatus called waitpid with WNOHANG,
  // it'll reap the process.  However, if GetTerminationStatus didn't
  // reap the child (because it was still running), we'll need to
  // Terminate via ProcessWatcher. So we can't close the handle here.
  if (termination_status_ != base::TERMINATION_STATUS_STILL_RUNNING)
    process_.Close();

  return termination_status_;
}

ChildProcessLauncher::Client* ChildProcessLauncher::ReplaceClientForTest(
    Client* client) {
  Client* ret = client_;
  client_ = client;
  return ret;
}

}  // namespace content
