// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

#if defined(OS_WIN)
#include "base/synchronization/waitable_event.h"
#elif defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

using content::BrowserChildProcessHostDelegate;
using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;
using content::ChildProcessHostImpl;

namespace {

static base::LazyInstance<BrowserChildProcessHostImpl::BrowserChildProcessList>
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

// Helper functions since the child process related notifications happen on the
// UI thread.
void ChildNotificationHelper(int notification_type,
                             const ChildProcessData& data) {
  content::NotificationService::current()->
        Notify(notification_type, content::NotificationService::AllSources(),
               content::Details<const ChildProcessData>(&data));
}

}  // namespace

namespace content {

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    ProcessType type,
    BrowserChildProcessHostDelegate* delegate) {
  return new BrowserChildProcessHostImpl(type, delegate);
}

#if defined(OS_MACOSX)
base::ProcessMetrics::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return MachBroker::GetInstance();
}
#endif

}  // namespace content

BrowserChildProcessHostImpl::BrowserChildProcessList*
    BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType type,
    BrowserChildProcessHostDelegate* delegate)
    : data_(type),
      delegate_(delegate),
#if !defined(OS_WIN)
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
#endif
      disconnect_was_alive_(false) {
  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  child_process_host_.reset(ChildProcessHost::Create(this));
  child_process_host_->AddFilter(new TraceMessageFilter);
  child_process_host_->AddFilter(new content::ProfilerMessageFilter(type));

  g_child_process_list.Get().push_back(this);
  content::GetContentClient()->browser()->BrowserChildProcessHostCreated(this);
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (BrowserChildProcessList::iterator it = copy.begin();
       it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

void BrowserChildProcessHostImpl::Launch(
#if defined(OS_WIN)
    const FilePath& exposed_dir,
#elif defined(OS_POSIX)
    bool use_zygote,
    const base::EnvironmentVector& environ,
#endif
    CommandLine* cmd_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  content::GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, data_.id);

  child_process_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
      exposed_dir,
#elif defined(OS_POSIX)
      use_zygote,
      environ,
      child_process_host_->TakeClientFileDescriptor(),
#endif
      cmd_line,
      this));
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return child_process_host_.get();
}

base::ProcessHandle BrowserChildProcessHostImpl::GetHandle() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetHandle())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetHandle();
}

void BrowserChildProcessHostImpl::SetName(const string16& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetHandle(base::ProcessHandle handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  data_.handle = handle;
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

void BrowserChildProcessHostImpl::SetTerminateChildOnShutdown(
    bool terminate_on_shutdown) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  child_process_->SetTerminateChildOnShutdown(terminate_on_shutdown);
}

void BrowserChildProcessHostImpl::Notify(int type) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ChildNotificationHelper, type, data_));
}

base::TerminationStatus BrowserChildProcessHostImpl::GetTerminationStatus(
    int* exit_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!child_process_.get())  // If the delegate doesn't use Launch() helper.
    return base::GetTerminationStatus(data_.handle, exit_code);
  return child_process_->GetChildTerminationStatus(exit_code);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32 peer_pid) {
  Notify(content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED);
  delegate_->OnChannelConnected(peer_pid);
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

bool BrowserChildProcessHostImpl::CanShutdown() {
  return delegate_->CanShutdown();
}

// Normally a ChildProcessHostDelegate deletes itself from this callback, but at
// this layer and below we need to have the final child process exit code to
// properly bucket crashes vs kills. On Windows we can do this if we wait until
// the process handle is signaled; on the rest of the platforms, we schedule a
// delayed task to wait for an exit code. However, this means that this method
// may be called twice: once from the actual channel error and once from
// OnWaitableEventSignaled() or the delayed task.
void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK(data_.handle != base::kNullProcessHandle);
  int exit_code;
  base::TerminationStatus status = GetTerminationStatus(&exit_code);
  switch (status) {
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
      delegate_->OnProcessCrashed(exit_code);
      // Report that this child process crashed.
      Notify(content::NOTIFICATION_CHILD_PROCESS_CRASHED);
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed",
                                data_.type,
                                content::PROCESS_TYPE_MAX);
      if (disconnect_was_alive_) {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.CrashedWasAlive",
                                  data_.type,
                                  content::PROCESS_TYPE_MAX);
      }
      break;
    }
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
      delegate_->OnProcessCrashed(exit_code);
      // Report that this child process was killed.
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed",
                                data_.type,
                                content::PROCESS_TYPE_MAX);
      if (disconnect_was_alive_) {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.KilledWasAlive",
                                  data_.type,
                                  content::PROCESS_TYPE_MAX);
      }
      break;
    }
    case base::TERMINATION_STATUS_STILL_RUNNING: {
      // Exit code not yet available. Ensure we don't wait forever for an exit
      // code.
      if (disconnect_was_alive_) {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive",
                                  data_.type,
                                  content::PROCESS_TYPE_MAX);
        break;
      }
      disconnect_was_alive_ = true;
#if defined(OS_WIN)
      child_watcher_.StartWatching(
          new base::WaitableEvent(data_.handle), this);
#else
      // On non-Windows platforms, give the child process some time to die after
      // disconnecting the channel so that the exit code and termination status
      // become available. This is best effort -- if the process doesn't die
      // within the time limit, this object gets destroyed.
      const base::TimeDelta kExitCodeWait =
          base::TimeDelta::FromMilliseconds(250);
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&BrowserChildProcessHostImpl::OnChildDisconnected,
                     task_factory_.GetWeakPtr()),
          kExitCodeWait);
#endif
      return;
    }

    default:
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected",
                            data_.type,
                            content::PROCESS_TYPE_MAX);
  // Notify in the main loop of the disconnection.
  Notify(content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED);
  delete delegate_;  // Will delete us
}

// The child process handle has been signaled so the exit code is finally
// available. Unfortunately STILL_ACTIVE (0x103) is a valid exit code in
// which case we should not call OnChildDisconnected() or else we will be
// waiting forever.
void BrowserChildProcessHostImpl::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined (OS_WIN)
  unsigned long exit_code = 0;
  GetExitCodeProcess(waitable_event->Release(), &exit_code);
  delete waitable_event;
  if (exit_code == STILL_ACTIVE) {
    delete delegate_;  // Will delete us
  } else {
    BrowserChildProcessHostImpl::OnChildDisconnected();
  }
#endif
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  if (!child_process_->GetHandle()) {
    delete delegate_;  // Will delete us
    return;
  }
  data_.handle = child_process_->GetHandle();
  delegate_->OnProcessLaunched();
}
