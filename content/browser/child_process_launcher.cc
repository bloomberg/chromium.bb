// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>  // For std::pair.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

#if defined(OS_WIN)
#include "base/file_path.h"
#include "content/common/sandbox_policy.h"
#include "content/public/common/sandbox_init.h"
#elif defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#elif defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/sandboxed_process_launcher.h"
#elif defined(OS_POSIX)
#include "base/memory/singleton.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#endif

namespace content {

// Having the functionality of ChildProcessLauncher be in an internal
// ref counted object allows us to automatically terminate the process when the
// parent class destructs, while still holding on to state that we need.
class ChildProcessLauncher::Context
    : public base::RefCountedThreadSafe<ChildProcessLauncher::Context> {
 public:
  Context()
      : client_(NULL),
        client_thread_id_(BrowserThread::UI),
        termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
        exit_code_(RESULT_CODE_NORMAL_EXIT),
        starting_(true)
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
        , zygote_(false)
#endif
        {
#if defined(OS_POSIX)
          terminate_child_on_shutdown_ = !CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kChildCleanExit);
#else
          terminate_child_on_shutdown_ = true;
#endif
  }

  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_ANDROID)
      int ipcfd,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::EnvironmentVector& environ,
      int ipcfd,
#endif
      CommandLine* cmd_line,
      int child_process_id,
      Client* client) {
    client_ = client;

    CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));

#if defined(OS_ANDROID)
    // We need to close the client end of the IPC channel to reliably detect
    // child termination. We will close this fd after we create the child
    // process which is asynchronous on Android.
    ipcfd_ = ipcfd;
#endif
    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(
            &Context::LaunchInternal,
            make_scoped_refptr(this),
            client_thread_id_,
            child_process_id,
#if defined(OS_WIN)
            exposed_dir,
#elif defined(OS_ANDROID)
            ipcfd,
#elif defined(OS_POSIX)
            use_zygote,
            environ,
            ipcfd,
#endif
            cmd_line));
  }

#if defined(OS_ANDROID)
  static void OnSandboxedProcessStarted(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      base::ProcessHandle handle) {
    if (BrowserThread::CurrentlyOn(client_thread_id)) {
      // This is always invoked on the UI thread which is commonly the
      // |client_thread_id| so we can shortcut one PostTask.
      this_object->Notify(handle);
    } else {
      BrowserThread::PostTask(
          client_thread_id, FROM_HERE,
          base::Bind(
              &ChildProcessLauncher::Context::Notify,
              this_object,
              handle));
    }
  }
#endif

  void ResetClient() {
    // No need for locking as this function gets called on the same thread that
    // client_ would be used.
    CHECK(BrowserThread::CurrentlyOn(client_thread_id_));
    client_ = NULL;
  }

  void set_terminate_child_on_shutdown(bool terminate_on_shutdown) {
    terminate_child_on_shutdown_ = terminate_on_shutdown;
  }

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncher::Context>;
  friend class ChildProcessLauncher;

  ~Context() {
    Terminate();
  }

  static void LaunchInternal(
      // |this_object| is NOT thread safe. Only use it to post a task back.
      scoped_refptr<Context> this_object,
      BrowserThread::ID client_thread_id,
      int child_process_id,
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_ANDROID)
      int ipcfd,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::EnvironmentVector& env,
      int ipcfd,
#endif
      CommandLine* cmd_line) {
    scoped_ptr<CommandLine> cmd_line_deleter(cmd_line);

#if defined(OS_WIN)
    base::ProcessHandle handle = StartProcessWithAccess(cmd_line, exposed_dir);
#elif defined(OS_ANDROID)
    // Android WebView runs in single process, ensure that we never get here
    // when running in single process mode.
    CHECK(!cmd_line->HasSwitch(switches::kSingleProcess));

    std::string process_type =
        cmd_line->GetSwitchValueASCII(switches::kProcessType);
    std::vector<FileDescriptorInfo> files_to_register;
    files_to_register.push_back(
        FileDescriptorInfo(kPrimaryIPCChannel,
                                    base::FileDescriptor(ipcfd, false)));

    GetContentClient()->browser()->
        GetAdditionalMappedFilesForChildProcess(*cmd_line, child_process_id,
                                                &files_to_register);

    StartSandboxedProcess(cmd_line->argv(), files_to_register,
        base::Bind(&ChildProcessLauncher::Context::OnSandboxedProcessStarted,
                   this_object, client_thread_id));

#elif defined(OS_POSIX)
    base::ProcessHandle handle = base::kNullProcessHandle;
    // We need to close the client end of the IPC channel to reliably detect
    // child termination.
    file_util::ScopedFD ipcfd_closer(&ipcfd);

    std::string process_type =
        cmd_line->GetSwitchValueASCII(switches::kProcessType);
    std::vector<FileDescriptorInfo> files_to_register;
    files_to_register.push_back(
        FileDescriptorInfo(kPrimaryIPCChannel,
                                    base::FileDescriptor(ipcfd, false)));

#if !defined(OS_MACOSX)
    GetContentClient()->browser()->
        GetAdditionalMappedFilesForChildProcess(*cmd_line, child_process_id,
                                                &files_to_register);
    if (use_zygote) {
      handle = ZygoteHostImpl::GetInstance()->ForkRequest(cmd_line->argv(),
                                                          files_to_register,
                                                          process_type);
    } else
    // Fall through to the normal posix case below when we're not zygoting.
#endif  // !defined(OS_MACOSX)
    {
      // Convert FD mapping to FileHandleMappingVector
      base::FileHandleMappingVector fds_to_map;
      for (size_t i = 0; i < files_to_register.size(); ++i) {
        fds_to_map.push_back(std::make_pair(
            files_to_register[i].fd.fd,
            files_to_register[i].id +
                base::GlobalDescriptors::kBaseDescriptor));
      }

#if !defined(OS_MACOSX)
      if (process_type == switches::kRendererProcess) {
        const int sandbox_fd =
            RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
        fds_to_map.push_back(std::make_pair(
            sandbox_fd,
            kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
      }
#endif  // defined(OS_MACOSX)

      // Actually launch the app.
      base::LaunchOptions options;
      options.environ = &env;
      options.fds_to_remap = &fds_to_map;

#if defined(OS_MACOSX)
      // Use synchronization to make sure that the MachBroker is ready to
      // receive a check-in from the new process before the new process
      // actually tries to check in.
      base::LaunchSynchronizationHandle synchronization_handle;
      options.synchronize = &synchronization_handle;
#endif  // defined(OS_MACOSX)

      bool launched = base::LaunchProcess(*cmd_line, options, &handle);

#if defined(OS_MACOSX)
      if (launched) {
        MachBroker* broker = MachBroker::GetInstance();
        {
          base::AutoLock lock(broker->GetLock());

          // Make sure the MachBroker is running, and inform it to expect a
          // check-in from the new process.
          broker->EnsureRunning();
          broker->AddPlaceholderForPid(handle);
        }

        // Now that the MachBroker is ready, the child may continue.
        base::LaunchSynchronize(synchronization_handle);
      }
#endif  // defined(OS_MACOSX)

      if (!launched)
        handle = base::kNullProcessHandle;
    }
#endif  // else defined(OS_POSIX)
#if !defined(OS_ANDROID)
  BrowserThread::PostTask(
      client_thread_id, FROM_HERE,
      base::Bind(
          &Context::Notify,
          this_object.get(),
#if defined(OS_POSIX) && !defined(OS_MACOSX)
          use_zygote,
#endif
          handle));
#endif  // !defined(OS_ANDROID)
  }

  void Notify(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::ProcessHandle handle) {
#if defined(OS_ANDROID)
    // Finally close the ipcfd
    file_util::ScopedFD ipcfd_closer(&ipcfd_);
#endif
    starting_ = false;
    process_.set_handle(handle);
    if (!handle)
      LOG(ERROR) << "Failed to launch child process";

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
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

    if (!terminate_child_on_shutdown_)
      return;

    // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!  So
    // don't this on the UI/IO threads.
    BrowserThread::PostTask(
        BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(
            &Context::TerminateInternal,
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
            zygote_,
#endif
            process_.handle()));
    process_.set_handle(base::kNullProcessHandle);
  }

  static void SetProcessBackgrounded(base::ProcessHandle handle,
                                     bool background) {
    base::Process process(handle);
    process.SetProcessBackgrounded(background);
  }

  static void TerminateInternal(
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
      bool zygote,
#endif
      base::ProcessHandle handle) {
#if defined(OS_ANDROID)
    LOG(INFO) << "ChromeProcess: Stopping process with handle " << handle;
    StopSandboxedProcess(handle);
#else
    base::Process process(handle);
     // Client has gone away, so just kill the process.  Using exit code 0
    // means that UMA won't treat this as a crash.
    process.Terminate(RESULT_CODE_NORMAL_EXIT);
    // On POSIX, we must additionally reap the child.
#if defined(OS_POSIX)
#if !defined(OS_MACOSX)
    if (zygote) {
      // If the renderer was created via a zygote, we have to proxy the reaping
      // through the zygote process.
      ZygoteHostImpl::GetInstance()->EnsureProcessTerminated(handle);
    } else
#endif  // !OS_MACOSX
    {
      base::EnsureProcessTerminated(handle);
    }
#endif  // OS_POSIX
    process.Close();
#endif  // defined(OS_ANDROID)
  }

  Client* client_;
  BrowserThread::ID client_thread_id_;
  base::Process process_;
  base::TerminationStatus termination_status_;
  int exit_code_;
  bool starting_;
  // Controls whether the child process should be terminated on browser
  // shutdown. Default behavior is to terminate the child.
  bool terminate_child_on_shutdown_;
#if defined(OS_ANDROID)
  // The fd to close after creating the process.
  int ipcfd_;
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  bool zygote_;
#endif
};


ChildProcessLauncher::ChildProcessLauncher(
#if defined(OS_WIN)
    const FilePath& exposed_dir,
#elif defined(OS_POSIX)
    bool use_zygote,
    const base::EnvironmentVector& environ,
    int ipcfd,
#endif
    CommandLine* cmd_line,
    int child_process_id,
    Client* client) {
  context_ = new Context();
  context_->Launch(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_ANDROID)
      ipcfd,
#elif defined(OS_POSIX)
      use_zygote,
      environ,
      ipcfd,
#endif
      cmd_line,
      child_process_id,
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
    bool known_dead,
    int* exit_code) {
  base::ProcessHandle handle = context_->process_.handle();
  if (handle == base::kNullProcessHandle) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = context_->exit_code_;
    return context_->termination_status_;
  }
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  if (context_->zygote_) {
    context_->termination_status_ = ZygoteHostImpl::GetInstance()->
        GetTerminationStatus(handle, known_dead, &context_->exit_code_);
  } else
#endif
  {
    context_->termination_status_ =
        base::GetTerminationStatus(handle, &context_->exit_code_);
  }

  if (exit_code)
    *exit_code = context_->exit_code_;

  // POSIX: If the process crashed, then the kernel closed the socket
  // for it and so the child has already died by the time we get
  // here. Since GetTerminationStatus called waitpid with WNOHANG,
  // it'll reap the process.  However, if GetTerminationStatus didn't
  // reap the child (because it was still running), we'll need to
  // Terminate via ProcessWatcher. So we can't close the handle here.
  if (context_->termination_status_ != base::TERMINATION_STATUS_STILL_RUNNING)
    context_->process_.Close();

  return context_->termination_status_;
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(
          &ChildProcessLauncher::Context::SetProcessBackgrounded,
          GetHandle(), background));
}

void ChildProcessLauncher::SetTerminateChildOnShutdown(
  bool terminate_on_shutdown) {
  if (context_)
    context_->set_terminate_child_on_shutdown(terminate_on_shutdown);
}

}  // namespace content
