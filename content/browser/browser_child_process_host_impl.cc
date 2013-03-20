// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/histogram_message_filter.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_messages.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

namespace content {
namespace {

static base::LazyInstance<BrowserChildProcessHostImpl::BrowserChildProcessList>
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<ObserverList<BrowserChildProcessObserver> >
    g_observers = LAZY_INSTANCE_INITIALIZER;

void NotifyProcessHostConnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostConnected(data));
}

void NotifyProcessHostDisconnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostDisconnected(data));
}

void NotifyProcessCrashed(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessCrashed(data));
}

}  // namespace

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

// static
BrowserChildProcessHostImpl::BrowserChildProcessList*
    BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

// static
void BrowserChildProcessHostImpl::AddObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_observers.Get().AddObserver(observer);
}

// static
void BrowserChildProcessHostImpl::RemoveObserver(
    BrowserChildProcessObserver* observer) {
  // TODO(phajdan.jr): Check thread after fixing http://crbug.com/167126.
  g_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    ProcessType type,
    BrowserChildProcessHostDelegate* delegate)
    : data_(type),
      delegate_(delegate) {
  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  child_process_host_.reset(ChildProcessHost::Create(this));
  child_process_host_->AddFilter(new TraceMessageFilter);
  child_process_host_->AddFilter(new ProfilerMessageFilter(type));
  child_process_host_->AddFilter(new HistogramMessageFilter());

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);

#if defined(OS_WIN)
  DeleteProcessWaitableEvent(early_exit_watcher_.GetWatchedEvent());
#endif
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
    SandboxedProcessLauncherDelegate* delegate,
#elif defined(OS_POSIX)
    bool use_zygote,
    const base::EnvironmentVector& environ,
#endif
    CommandLine* cmd_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, data_.id);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  static const char* kForwardSwitches[] = {
    switches::kDisableLogging,
    switches::kEnableDCHECK,
    switches::kEnableLogging,
    switches::kLoggingLevel,
    switches::kV,
    switches::kVModule,
#if defined(OS_POSIX)
    switches::kChildCleanExit,
#endif
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches,
                             arraysize(kForwardSwitches));

  child_process_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
      delegate,
#elif defined(OS_POSIX)
      use_zygote,
      environ,
      child_process_host_->TakeClientFileDescriptor(),
#endif
      cmd_line,
      data_.id,
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

void BrowserChildProcessHostImpl::NotifyProcessInstanceCreated(
    const ChildProcessData& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessInstanceCreated(data));
}

base::TerminationStatus BrowserChildProcessHostImpl::GetTerminationStatus(
    int* exit_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!child_process_.get())  // If the delegate doesn't use Launch() helper.
    return base::GetTerminationStatus(data_.handle, exit_code);
  return child_process_->GetChildTerminationStatus(false /* known_dead */,
                                                   exit_code);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32 peer_pid) {
#if defined(OS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel.
  DeleteProcessWaitableEvent(early_exit_watcher_.GetWatchedEvent());
  early_exit_watcher_.StopWatching();
#endif

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyProcessHostConnected, data_));

  delegate_->OnChannelConnected(peer_pid);
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

bool BrowserChildProcessHostImpl::CanShutdown() {
  return delegate_->CanShutdown();
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(data_.handle != base::kNullProcessHandle);
  int exit_code;
  base::TerminationStatus status = GetTerminationStatus(&exit_code);
  switch (status) {
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
      delegate_->OnProcessCrashed(exit_code);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&NotifyProcessCrashed, data_));
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed",
                                data_.type,
                                PROCESS_TYPE_MAX);
      break;
    }
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
      delegate_->OnProcessCrashed(exit_code);
      // Report that this child process was killed.
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed",
                                data_.type,
                                PROCESS_TYPE_MAX);
      break;
    }
    case base::TERMINATION_STATUS_STILL_RUNNING: {
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive",
                                data_.type,
                                PROCESS_TYPE_MAX);
    }
    default:
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected",
                            data_.type,
                            PROCESS_TYPE_MAX);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyProcessHostDisconnected, data_));
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  if (!child_process_->GetHandle()) {
    delete delegate_;  // Will delete us
    return;
  }

#if defined(OS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedEvent());
  early_exit_watcher_.StartWatching(
      new base::WaitableEvent(child_process_->GetHandle()),
      base::Bind(&BrowserChildProcessHostImpl::OnProcessExitedEarly,
                 base::Unretained(this)));
#endif

  data_.handle = child_process_->GetHandle();
  delegate_->OnProcessLaunched();
}

#if defined(OS_WIN)

void BrowserChildProcessHostImpl::DeleteProcessWaitableEvent(
    base::WaitableEvent* event) {
  if (!event)
    return;

  // The WaitableEvent does not own the process handle so ensure it does not
  // close it.
  event->Release();

  delete event;
}

void BrowserChildProcessHostImpl::OnProcessExitedEarly(
    base::WaitableEvent* event) {
  DeleteProcessWaitableEvent(event);
  OnChildDisconnected();
}

#endif

}  // namespace content
