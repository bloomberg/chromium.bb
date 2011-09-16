// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/content_switches.h"
#include "content/common/notification_service.h"
#include "content/common/plugin_messages.h"
#include "content/common/process_watcher.h"
#include "content/common/result_codes.h"

#if defined(OS_WIN)
#include "base/synchronization/waitable_event.h"
#endif  // OS_WIN

namespace {

typedef std::list<BrowserChildProcessHost*> ChildProcessList;
static base::LazyInstance<ChildProcessList> g_child_process_list(
    base::LINKER_INITIALIZED);

// The NotificationTask is used to notify about plugin process connection/
// disconnection. It is needed because the notifications in the
// NotificationService must happen in the main thread.
class ChildNotificationTask : public Task {
 public:
  ChildNotificationTask(
      int notification_type, ChildProcessInfo* info)
      : notification_type_(notification_type), info_(*info) { }

  virtual void Run() {
    NotificationService::current()->
        Notify(notification_type_, NotificationService::AllSources(),
               Details<ChildProcessInfo>(&info_));
  }

 private:
  int notification_type_;
  ChildProcessInfo info_;
};

}  // namespace

BrowserChildProcessHost::BrowserChildProcessHost(
    ChildProcessInfo::ProcessType type)
    : ChildProcessInfo(type, -1),
      ALLOW_THIS_IN_INITIALIZER_LIST(client_(this)),
      disconnect_was_alive_(false) {
  AddFilter(new TraceMessageFilter);

  g_child_process_list.Get().push_back(this);
}

BrowserChildProcessHost::~BrowserChildProcessHost() {
  g_child_process_list.Get().remove(this);
}

// static
void BrowserChildProcessHost::TerminateAll() {
  // Make a copy since the ChildProcessHost dtor mutates the original list.
  ChildProcessList copy = g_child_process_list.Get();
  STLDeleteElements(&copy);
}

void BrowserChildProcessHost::Launch(
#if defined(OS_WIN)
    const FilePath& exposed_dir,
#elif defined(OS_POSIX)
    bool use_zygote,
    const base::environment_vector& environ,
#endif
    CommandLine* cmd_line) {

  content::GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, id());

  child_process_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      use_zygote,
      environ,
      channel()->GetClientFileDescriptor(),
#endif
      cmd_line,
      &client_));
}

base::ProcessHandle BrowserChildProcessHost::GetChildProcessHandle() const {
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetHandle())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetHandle();
}

void BrowserChildProcessHost::ForceShutdown() {
  g_child_process_list.Get().remove(this);
  ChildProcessHost::ForceShutdown();
}

void BrowserChildProcessHost::SetTerminateChildOnShutdown(
  bool terminate_on_shutdown) {
  child_process_->SetTerminateChildOnShutdown(terminate_on_shutdown);
}

void BrowserChildProcessHost::Notify(int type) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, new ChildNotificationTask(type, this));
}

base::TerminationStatus BrowserChildProcessHost::GetChildTerminationStatus(
    int* exit_code) {
  return child_process_->GetChildTerminationStatus(exit_code);
}

// The ChildProcessHost default implementation calls OnChildDied() always
// but at this layer and below we need to have the final child process exit
// code to properly bucket crashes vs kills. At least on Windows we can do
// this if we wait until the process handle is signaled, however this means
// that this function can be called twice: once from the actual channel error
// and once from OnWaitableEventSignaled().
void BrowserChildProcessHost::OnChildDisconnected() {
  DCHECK(handle() != base::kNullProcessHandle);
  int exit_code;
  base::TerminationStatus status = GetChildTerminationStatus(&exit_code);
  switch (status) {
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
      OnProcessCrashed(exit_code);
      // Report that this child process crashed.
      Notify(content::NOTIFICATION_CHILD_PROCESS_CRASHED);
      UMA_HISTOGRAM_COUNTS("ChildProcess.Crashes", this->type());
      if (disconnect_was_alive_) {
        UMA_HISTOGRAM_COUNTS("ChildProcess.CrashesWasAlive", this->type());
      }
      break;
    }
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
      OnProcessWasKilled(exit_code);
      // Report that this child process was killed.
      Notify(content::NOTIFICATION_CHILD_PROCESS_WAS_KILLED);
      UMA_HISTOGRAM_COUNTS("ChildProcess.Kills", this->type());
      if (disconnect_was_alive_) {
        UMA_HISTOGRAM_COUNTS("ChildProcess.KillsWasAlive", this->type());
      }
      break;
    }
    case base::TERMINATION_STATUS_STILL_RUNNING: {
      // exit code not yet available.
      disconnect_was_alive_ = true;
#if defined(OS_WIN)
      child_watcher_.StartWatching(new base::WaitableEvent(handle()), this);
      return;
#else
      break;
#endif
    }

    default:
      break;
  }
  // Notify in the main loop of the disconnection.
  Notify(content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED);
  OnChildDied();
}

// The child process handle has been signaled so the exit code is finally
// available. Unfortunately STILL_ACTIVE (0x103) is a valid exit code in
// which case we should not call OnChildDisconnected() or else we will be
// waiting forever.
void BrowserChildProcessHost::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined (OS_WIN)
  unsigned long exit_code = 0;
  GetExitCodeProcess(waitable_event->Release(), &exit_code);
  delete waitable_event;
  if (exit_code == STILL_ACTIVE)
    OnChildDied();
  BrowserChildProcessHost::OnChildDisconnected();
#endif
}

void BrowserChildProcessHost::ShutdownStarted() {
  // Must remove the process from the list now, in case it gets used for a
  // new instance before our watcher tells us that the process terminated.
  g_child_process_list.Get().remove(this);
}

BrowserChildProcessHost::ClientHook::ClientHook(BrowserChildProcessHost* host)
    : host_(host) {
}

void BrowserChildProcessHost::ClientHook::OnProcessLaunched() {
  if (!host_->child_process_->GetHandle()) {
    host_->OnChildDied();
    return;
  }
  host_->set_handle(host_->child_process_->GetHandle());
  host_->OnProcessLaunched();
}

BrowserChildProcessHost::Iterator::Iterator()
    : all_(true), type_(UNKNOWN_PROCESS) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
  iterator_ = g_child_process_list.Get().begin();
}

BrowserChildProcessHost::Iterator::Iterator(ChildProcessInfo::ProcessType type)
    : all_(false), type_(type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
  iterator_ = g_child_process_list.Get().begin();
  if (!Done() && (*iterator_)->type() != type_)
    ++(*this);
}

BrowserChildProcessHost* BrowserChildProcessHost::Iterator::operator++() {
  do {
    ++iterator_;
    if (Done())
      break;

    if (!all_ && (*iterator_)->type() != type_)
      continue;

    return *iterator_;
  } while (true);

  return NULL;
}

bool BrowserChildProcessHost::Iterator::Done() {
  return iterator_ == g_child_process_list.Get().end();
}
