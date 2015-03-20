// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>  // For std::pair.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/profiler/scoped_tracker.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
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
#include "content/browser/bootstrap_sandbox_mac.h"
#include "content/browser/mach_broker_mac.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#elif defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/child_process_launcher_android.h"
#elif defined(OS_POSIX)
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/browser/file_descriptor_info_impl.h"
#endif

namespace content {

// Having the functionality of ChildProcessLauncher be in an internal
// ref counted object allows us to automatically terminate the process when the
// parent class destructs, while still holding on to state that we need.
class ChildProcessLauncher::Context
    : public base::RefCountedThreadSafe<ChildProcessLauncher::Context> {
 public:
  Context();

  // Posts a task to a dedicated thread to do the actual work.
  // Must be called on the UI thread.
  void Launch(SandboxedProcessLauncherDelegate* delegate,
              base::CommandLine* cmd_line,
              int child_process_id,
              Client* client);

  // Resets the client (the client is going away).
  void ResetClient();

  bool starting() const { return starting_; }

  const base::Process& process() const { return process_; }

  int exit_code() const { return exit_code_; }

  base::TerminationStatus termination_status() const {
    DCHECK_CURRENTLY_ON(client_thread_id_);
    return termination_status_;
  }

  void SetTerminateChildOnShutdown(bool terminate_on_shutdown) {
    DCHECK_CURRENTLY_ON(client_thread_id_);
    terminate_child_on_shutdown_ = terminate_on_shutdown;
  }

  void UpdateTerminationStatus(bool known_dead);

  void Close() {
    DCHECK_CURRENTLY_ON(client_thread_id_);
    process_.Close();
  }

  void SetProcessBackgrounded(bool background);

  BrowserThread::ID client_thread_id() const { return client_thread_id_; }

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncher::Context>;

  ~Context() {
    Terminate();
  }

#if defined(OS_ANDROID)
  static void OnChildProcessStartedAndroid(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      const base::TimeTicks begin_launch_time,
      base::ProcessHandle handle);
#endif

  static void RecordHistogramsOnLauncherThread(base::TimeDelta launch_time);

  // Performs the actual work of launching the process.
  // Runs on the PROCESS_LAUNCHER thread.
  static void LaunchOnLauncherThread(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      int child_process_id,
      SandboxedProcessLauncherDelegate* delegate,
      base::CommandLine* cmd_line);

  // Notifies the client about the result of the operation.
  // Runs on the UI thread.
  void Notify(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::Process process);

  void Terminate();

  static void SetProcessBackgroundedOnLauncherThread(base::Process process,
                                                     bool background);

  static void TerminateOnLauncherThread(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::Process process);

  Client* client_;
  BrowserThread::ID client_thread_id_;
  base::Process process_;
  base::TerminationStatus termination_status_;
  int exit_code_;
#if defined(OS_ANDROID)
  // The fd to close after creating the process.
  base::ScopedFD ipcfd_;
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  bool zygote_;
#endif
  bool starting_;
  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  bool terminate_child_on_shutdown_;
};

ChildProcessLauncher::Context::Context()
    : client_(NULL),
      termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
      exit_code_(RESULT_CODE_NORMAL_EXIT),
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      zygote_(false),
#endif
      starting_(true),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
      terminate_child_on_shutdown_(false) {
#else
      terminate_child_on_shutdown_(true) {
#endif
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));
}

void ChildProcessLauncher::Context::Launch(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    int child_process_id,
    Client* client) {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  client_ = client;

#if defined(OS_ANDROID)
  // Android only supports renderer, sandboxed utility and gpu.
  std::string process_type =
      cmd_line->GetSwitchValueASCII(switches::kProcessType);
  CHECK(process_type == switches::kGpuProcess ||
        process_type == switches::kRendererProcess ||
        process_type == switches::kUtilityProcess)
      << "Unsupported process type: " << process_type;

  // Non-sandboxed utility or renderer process are currently not supported.
  DCHECK(process_type == switches::kGpuProcess ||
         !cmd_line->HasSwitch(switches::kNoSandbox));

  // We need to close the client end of the IPC channel to reliably detect
  // child termination. We will close this fd after we create the child
  // process which is asynchronous on Android.
  ipcfd_.reset(delegate->TakeIpcFd().release());
#endif
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&Context::LaunchOnLauncherThread,
                 make_scoped_refptr(this),
                 client_thread_id_,
                 child_process_id,
                 delegate,
                 cmd_line));
}

#if defined(OS_ANDROID)
// TODO(sievers): Remove this by defining better what happens on what
// thread in the corresponding Java code.
void ChildProcessLauncher::Context::OnChildProcessStartedAndroid(
    // |this_object| is NOT thread safe. Only use it to post a task back.
    scoped_refptr<Context> this_object,
    BrowserThread::ID client_thread_id,
    const base::TimeTicks begin_launch_time,
    base::ProcessHandle handle) {
  // This can be called on the launcher thread or UI thread.
  base::TimeDelta launch_time = base::TimeTicks::Now() - begin_launch_time;
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(
          &ChildProcessLauncher::Context::RecordHistogramsOnLauncherThread,
          launch_time));

  if (BrowserThread::CurrentlyOn(client_thread_id)) {
    this_object->Notify(base::Process(handle));
  } else {
    BrowserThread::PostTask(
        client_thread_id, FROM_HERE,
        base::Bind(&ChildProcessLauncher::Context::Notify,
                   this_object,
                   base::Passed(base::Process(handle))));
  }
}
#endif

void ChildProcessLauncher::Context::ResetClient() {
  // No need for locking as this function gets called on the same thread that
  // client_ would be used.
  DCHECK_CURRENTLY_ON(client_thread_id_);
  client_ = NULL;
}

void ChildProcessLauncher::Context::UpdateTerminationStatus(bool known_dead) {
  DCHECK_CURRENTLY_ON(client_thread_id_);
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

void ChildProcessLauncher::Context::SetProcessBackgrounded(bool background) {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  base::Process to_pass = process_.Duplicate();
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&Context::SetProcessBackgroundedOnLauncherThread,
                 base::Passed(&to_pass), background));
}

// static
void ChildProcessLauncher::Context::RecordHistogramsOnLauncherThread(
    base::TimeDelta launch_time) {
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

// static
void ChildProcessLauncher::Context::LaunchOnLauncherThread(
    // |this_object| is NOT thread safe. Only use it to post a task back.
    scoped_refptr<Context> this_object,
    BrowserThread::ID client_thread_id,
    int child_process_id,
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  scoped_ptr<SandboxedProcessLauncherDelegate> delegate_deleter(delegate);
#if defined(OS_WIN)
  bool launch_elevated = delegate->ShouldLaunchElevated();
#elif defined(OS_ANDROID)
  // Uses |ipcfd_| instead of |ipcfd| on Android.
#elif defined(OS_MACOSX)
  base::EnvironmentMap env = delegate->GetEnvironment();
  base::ScopedFD ipcfd = delegate->TakeIpcFd();
#elif defined(OS_POSIX)
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
  files_to_register->Share(kPrimaryIPCChannel, this_object->ipcfd_.get());
#else
  files_to_register->Transfer(kPrimaryIPCChannel, ipcfd.Pass());
#endif
#endif

#if defined(OS_ANDROID)
  // Android WebView runs in single process, ensure that we never get here
  // when running in single process mode.
  CHECK(!cmd_line->HasSwitch(switches::kSingleProcess));

  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      *cmd_line, child_process_id, files_to_register.get());

  StartChildProcess(
      cmd_line->argv(), child_process_id, files_to_register.Pass(),
      base::Bind(
          &ChildProcessLauncher::Context::OnChildProcessStartedAndroid,
          this_object, client_thread_id, begin_launch_time));

#elif defined(OS_POSIX)
  // We need to close the client end of the IPC channel to reliably detect
  // child termination.

#if !defined(OS_MACOSX)
  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      *cmd_line, child_process_id, files_to_register.get());
  if (use_zygote) {
    base::ProcessHandle handle = ZygoteHostImpl::GetInstance()->ForkRequest(
        cmd_line->argv(), files_to_register.Pass(), process_type);
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

    const int bootstrap_sandbox_policy = delegate->GetSandboxType();
    if (ShouldEnableBootstrapSandbox() &&
        bootstrap_sandbox_policy != SANDBOX_TYPE_INVALID) {
      options.replacement_bootstrap_name =
          GetBootstrapSandbox()->server_bootstrap_name();
      GetBootstrapSandbox()->PrepareToForkWithPolicy(
          bootstrap_sandbox_policy);
    }
#endif  // defined(OS_MACOSX)

    process = base::LaunchProcess(*cmd_line, options);

#if defined(OS_MACOSX)
    if (ShouldEnableBootstrapSandbox() &&
        bootstrap_sandbox_policy != SANDBOX_TYPE_INVALID) {
      GetBootstrapSandbox()->FinishedFork(process.Handle());
    }

    if (process.IsValid())
      broker->AddPlaceholderForPid(process.Pid(), child_process_id);

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
  BrowserThread::PostTask(
      client_thread_id, FROM_HERE,
      base::Bind(&Context::Notify,
                 this_object.get(),
#if defined(OS_POSIX) && !defined(OS_MACOSX)
                 use_zygote,
#endif
                 base::Passed(&process)));
#endif  // !defined(OS_ANDROID)
}

void ChildProcessLauncher::Context::Notify(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    bool zygote,
#endif
    base::Process process) {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465841
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465841 ChildProcessLauncher::Context::Notify::Start"));

#if defined(OS_ANDROID)
  // Finally close the ipcfd
  base::ScopedFD ipcfd_closer = ipcfd_.Pass();
#endif
  starting_ = false;
  process_ = process.Pass();
  if (!process_.IsValid())
    LOG(ERROR) << "Failed to launch child process";

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  zygote_ = zygote;
#endif
  if (client_) {
    if (process_.IsValid()) {
      // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465841
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "465841 ChildProcessLauncher::Context::Notify::ProcessLaunched"));
      client_->OnProcessLaunched();
    } else {
      // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465841
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile3(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "465841 ChildProcessLauncher::Context::Notify::ProcessFailed"));
      client_->OnProcessLaunchFailed();
    }
  } else {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465841
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile4(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "465841 ChildProcessLauncher::Context::Notify::ProcessTerminate"));
    Terminate();
  }
}

void ChildProcessLauncher::Context::Terminate() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  if (!process_.IsValid())
    return;

  if (!terminate_child_on_shutdown_)
    return;

  // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
  // don't this on the UI/IO threads.
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&Context::TerminateOnLauncherThread,
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
                zygote_,
#endif
                base::Passed(&process_)));
}

// static
void ChildProcessLauncher::Context::SetProcessBackgroundedOnLauncherThread(
    base::Process process,
    bool background) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  process.SetProcessBackgrounded(background);
#if defined(OS_ANDROID)
  SetChildProcessInForeground(process.Handle(), !background);
#endif
}

// static
void ChildProcessLauncher::Context::TerminateOnLauncherThread(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    bool zygote,
#endif
    base::Process process) {
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
  base::EnsureProcessTerminated(process.Pass());
#endif  // OS_POSIX
#endif  // defined(OS_ANDROID)
}

// -----------------------------------------------------------------------------

ChildProcessLauncher::ChildProcessLauncher(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    int child_process_id,
    Client* client) {
  context_ = new Context();
  context_->Launch(
      delegate,
      cmd_line,
      child_process_id,
      client);
}

ChildProcessLauncher::~ChildProcessLauncher() {
  context_->ResetClient();
}

bool ChildProcessLauncher::IsStarting() {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CURRENTLY_ON(context_->client_thread_id());
  return context_->starting();
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CURRENTLY_ON(context_->client_thread_id());
  DCHECK(!context_->starting());
  return context_->process();
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    bool known_dead,
    int* exit_code) {
  DCHECK_CURRENTLY_ON(context_->client_thread_id());
  if (!context_->process().IsValid()) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = context_->exit_code();
    return context_->termination_status();
  }

  context_->UpdateTerminationStatus(known_dead);
  if (exit_code)
    *exit_code = context_->exit_code();

  // POSIX: If the process crashed, then the kernel closed the socket
  // for it and so the child has already died by the time we get
  // here. Since GetTerminationStatus called waitpid with WNOHANG,
  // it'll reap the process.  However, if GetTerminationStatus didn't
  // reap the child (because it was still running), we'll need to
  // Terminate via ProcessWatcher. So we can't close the handle here.
  if (context_->termination_status() != base::TERMINATION_STATUS_STILL_RUNNING)
    context_->Close();

  return context_->termination_status();
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  context_->SetProcessBackgrounded(background);
}

void ChildProcessLauncher::SetTerminateChildOnShutdown(
    bool terminate_on_shutdown) {
  if (context_.get())
    context_->SetTerminateChildOnShutdown(terminate_on_shutdown);
}

}  // namespace content
