// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>  // For std::pair.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"
#include "content/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/file_path.h"
#include "chrome/common/sandbox_policy.h"
#elif defined(OS_LINUX)
#include "base/singleton.h"
#include "chrome/browser/crash_handler_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/mach_broker_mac.h"
#endif

#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif

// Having the functionality of ChildProcessLauncher be in an internal
// ref counted object allows us to automatically terminate the process when the
// parent class destructs, while still holding on to state that we need.
class ChildProcessLauncher::Context
    : public base::RefCountedThreadSafe<ChildProcessLauncher::Context> {
 public:
  Context()
      : client_(NULL),
        client_thread_id_(BrowserThread::UI),
        starting_(true)
#if defined(OS_LINUX)
        , zygote_(false)
#endif
        {
  }

  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& environ,
      int ipcfd,
#endif
      CommandLine* cmd_line,
      Client* client) {
    client_ = client;

    CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));

    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        NewRunnableMethod(
            this,
            &Context::LaunchInternal,
#if defined(OS_WIN)
            exposed_dir,
#elif defined(OS_POSIX)
            use_zygote,
            environ,
            ipcfd,
#endif
            cmd_line));
  }

  void ResetClient() {
    // No need for locking as this function gets called on the same thread that
    // client_ would be used.
    CHECK(BrowserThread::CurrentlyOn(client_thread_id_));
    client_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncher::Context>;
  friend class ChildProcessLauncher;

  ~Context() {
    Terminate();
  }

  void LaunchInternal(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& env,
      int ipcfd,
#endif
      CommandLine* cmd_line) {
    scoped_ptr<CommandLine> cmd_line_deleter(cmd_line);
    base::ProcessHandle handle = base::kNullProcessHandle;
#if defined(OS_WIN)
    handle = sandbox::StartProcessWithAccess(cmd_line, exposed_dir);
#elif defined(OS_POSIX)

#if defined(OS_LINUX)
    if (use_zygote) {
      base::GlobalDescriptors::Mapping mapping;
      mapping.push_back(std::pair<uint32_t, int>(kPrimaryIPCChannel, ipcfd));
      const int crash_signal_fd =
          RendererCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();
      if (crash_signal_fd >= 0) {
        mapping.push_back(std::pair<uint32_t, int>(kCrashDumpSignal,
                                                   crash_signal_fd));
      }
      handle = ZygoteHost::GetInstance()->ForkRenderer(cmd_line->argv(),
                                                       mapping);
    } else
    // Fall through to the normal posix case below when we're not zygoting.
#endif
    {
      base::file_handle_mapping_vector fds_to_map;
      fds_to_map.push_back(std::make_pair(
          ipcfd,
          kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));

#if defined(OS_LINUX)
      // On Linux, we need to add some extra file descriptors for crash handling
      // and the sandbox.
      bool is_renderer =
          cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kRendererProcess;
      bool is_plugin =
          cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kPluginProcess;
      bool is_gpu =
          cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kGpuProcess;

      if (is_renderer || is_plugin || is_gpu) {
        int crash_signal_fd;
        if (is_renderer) {
          crash_signal_fd = RendererCrashHandlerHostLinux::GetInstance()->
              GetDeathSignalSocket();
        } else if (is_plugin) {
          crash_signal_fd = PluginCrashHandlerHostLinux::GetInstance()->
              GetDeathSignalSocket();
        } else {
          crash_signal_fd = GpuCrashHandlerHostLinux::GetInstance()->
              GetDeathSignalSocket();
        }
        if (crash_signal_fd >= 0) {
          fds_to_map.push_back(std::make_pair(
              crash_signal_fd,
              kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor));
        }
      }
      if (is_renderer) {
        const int sandbox_fd =
            RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
        fds_to_map.push_back(std::make_pair(
            sandbox_fd,
            kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
      }
#endif  // defined(OS_LINUX)

      bool launched = false;
#if defined(OS_MACOSX)
      // It is possible for the child process to die immediately after
      // launching.  To prevent leaking MachBroker map entries in this case,
      // lock around all of LaunchApp().  If the child dies, the death
      // notification will be processed by the MachBroker after the call to
      // AddPlaceholderForPid(), enabling proper cleanup.
      {  // begin scope for AutoLock
        MachBroker* broker = MachBroker::GetInstance();
        base::AutoLock lock(broker->GetLock());

        // This call to |PrepareForFork()| will start the MachBroker listener
        // thread, if it is not already running.  Therefore the browser process
        // will be listening for Mach IPC before LaunchApp() is called.
        broker->PrepareForFork();
#endif
        // Actually launch the app.
        launched = base::LaunchApp(cmd_line->argv(), env, fds_to_map,
                                   /* wait= */false, &handle);
#if defined(OS_MACOSX)
        if (launched)
          broker->AddPlaceholderForPid(handle);
      }  // end scope for AutoLock
#endif
      if (!launched)
        handle = base::kNullProcessHandle;
    }
#endif  // else defined(OS_POSIX)

    BrowserThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(
            this,
            &ChildProcessLauncher::Context::Notify,
#if defined(OS_LINUX)
            use_zygote,
#endif
            handle));
  }

  void Notify(
#if defined(OS_LINUX)
      bool zygote,
#endif
      base::ProcessHandle handle) {
    starting_ = false;
    process_.set_handle(handle);
#if defined(OS_LINUX)
    zygote_ = zygote;
#endif
    if (client_) {
      client_->OnProcessLaunched();
    } else {
      Terminate();
    }
  }

  void Terminate() {
    if (!process_.handle())
      return;

    // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
    // don't this on the UI/IO threads.
    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        NewRunnableFunction(
            &ChildProcessLauncher::Context::TerminateInternal,
#if defined(OS_LINUX)
            zygote_,
#endif
            process_.handle()));
    process_.set_handle(base::kNullProcessHandle);
  }

  static void TerminateInternal(
#if defined(OS_LINUX)
      bool zygote,
#endif
      base::ProcessHandle handle) {
    base::Process process(handle);
     // Client has gone away, so just kill the process.  Using exit code 0
    // means that UMA won't treat this as a crash.
    process.Terminate(ResultCodes::NORMAL_EXIT);
    // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
#if defined(OS_LINUX)
    if (zygote) {
      // If the renderer was created via a zygote, we have to proxy the reaping
      // through the zygote process.
      ZygoteHost::GetInstance()->EnsureProcessTerminated(handle);
    } else
#endif  // OS_LINUX
    {
      ProcessWatcher::EnsureProcessTerminated(handle);
    }
#endif  // OS_POSIX
    process.Close();
  }

  Client* client_;
  BrowserThread::ID client_thread_id_;
  base::Process process_;
  bool starting_;

#if defined(OS_LINUX)
  bool zygote_;
#endif
};


ChildProcessLauncher::ChildProcessLauncher(
#if defined(OS_WIN)
    const FilePath& exposed_dir,
#elif defined(OS_POSIX)
    bool use_zygote,
    const base::environment_vector& environ,
    int ipcfd,
#endif
    CommandLine* cmd_line,
    Client* client) {
  context_ = new Context();
  context_->Launch(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      use_zygote,
      environ,
      ipcfd,
#endif
      cmd_line,
      client);
}

ChildProcessLauncher::~ChildProcessLauncher() {
  context_->ResetClient();
}

bool ChildProcessLauncher::IsStarting() {
  return context_->starting_;
}

base::ProcessHandle ChildProcessLauncher::GetHandle() {
  DCHECK(!context_->starting_);
  return context_->process_.handle();
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    int* exit_code) {
  base::TerminationStatus status;
  base::ProcessHandle handle = context_->process_.handle();
#if defined(OS_LINUX)
  if (context_->zygote_) {
    status = ZygoteHost::GetInstance()->GetTerminationStatus(handle, exit_code);
  } else
#endif
  {
    status = base::GetTerminationStatus(handle, exit_code);
  }

  // POSIX: If the process crashed, then the kernel closed the socket
  // for it and so the child has already died by the time we get
  // here. Since GetTerminationStatus called waitpid with WNOHANG,
  // it'll reap the process.  However, if GetTerminationStatus didn't
  // reap the child (because it was still running), we'll need to
  // Terminate via ProcessWatcher. So we can't close the handle here.
  if (status != base::TERMINATION_STATUS_STILL_RUNNING)
    context_->process_.Close();

  return status;
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  DCHECK(!context_->starting_);
  context_->process_.SetProcessBackgrounded(background);
}
