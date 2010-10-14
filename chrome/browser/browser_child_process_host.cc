// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_child_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif  // OS_LINUX

namespace {

typedef std::list<BrowserChildProcessHost*> ChildProcessList;

// The NotificationTask is used to notify about plugin process connection/
// disconnection. It is needed because the notifications in the
// NotificationService must happen in the main thread.
class ChildNotificationTask : public Task {
 public:
  ChildNotificationTask(
      NotificationType notification_type, ChildProcessInfo* info)
      : notification_type_(notification_type), info_(*info) { }

  virtual void Run() {
    NotificationService::current()->
        Notify(notification_type_, NotificationService::AllSources(),
               Details<ChildProcessInfo>(&info_));
  }

 private:
  NotificationType notification_type_;
  ChildProcessInfo info_;
};

}  // namespace


BrowserChildProcessHost::BrowserChildProcessHost(
    ProcessType type, ResourceDispatcherHost* resource_dispatcher_host)
    : Receiver(type, -1),
      ALLOW_THIS_IN_INITIALIZER_LIST(client_(this)),
      resource_dispatcher_host_(resource_dispatcher_host) {
  Singleton<ChildProcessList>::get()->push_back(this);
}


BrowserChildProcessHost::~BrowserChildProcessHost() {
  Singleton<ChildProcessList>::get()->remove(this);

  if (resource_dispatcher_host_)
    resource_dispatcher_host_->CancelRequestsForProcess(id());
}

// static
void BrowserChildProcessHost::SetCrashReporterCommandLine(
    CommandLine* command_line) {
#if defined(USE_LINUX_BREAKPAD)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
        child_process_logging::GetClientId() + "," + base::GetLinuxDistro());
  }
#elif defined(OS_MACOSX)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    child_process_logging::GetClientId());
  }
#endif  // OS_MACOSX
}

// static
void BrowserChildProcessHost::TerminateAll() {
  // Make a copy since the ChildProcessHost dtor mutates the original list.
  ChildProcessList copy = *(Singleton<ChildProcessList>::get());
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

bool BrowserChildProcessHost::Send(IPC::Message* msg) {
  return SendOnChannel(msg);
}

void BrowserChildProcessHost::ForceShutdown() {
  Singleton<ChildProcessList>::get()->remove(this);
  ChildProcessHost::ForceShutdown();
}

void BrowserChildProcessHost::Notify(NotificationType type) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, new ChildNotificationTask(type, this));
}

bool BrowserChildProcessHost::DidChildCrash() {
  return child_process_->DidProcessCrash();
}

void BrowserChildProcessHost::OnChildDied() {
  if (handle() != base::kNullProcessHandle) {
    bool did_crash = DidChildCrash();
    if (did_crash) {
      OnProcessCrashed();
      // Report that this child process crashed.
      Notify(NotificationType::CHILD_PROCESS_CRASHED);
      UMA_HISTOGRAM_COUNTS("ChildProcess.Crashes", this->type());
    }
    // Notify in the main loop of the disconnection.
    Notify(NotificationType::CHILD_PROCESS_HOST_DISCONNECTED);
  }
  ChildProcessHost::OnChildDied();
}

bool BrowserChildProcessHost::InterceptMessageFromChild(
    const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = false;
  if (resource_dispatcher_host_) {
    handled = resource_dispatcher_host_->OnMessageReceived(
        msg, this, &msg_is_ok);
  }
  if (!handled && (msg.type() == PluginProcessHostMsg_ShutdownRequest::ID)) {
    // Must remove the process from the list now, in case it gets used for a
    // new instance before our watcher tells us that the process terminated.
    Singleton<ChildProcessList>::get()->remove(this);
  }
  if (!msg_is_ok)
    base::KillProcess(handle(), ResultCodes::KILLED_BAD_MESSAGE, false);
  return handled;
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
  iterator_ = Singleton<ChildProcessList>::get()->begin();
}

BrowserChildProcessHost::Iterator::Iterator(ProcessType type)
    : all_(false), type_(type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
  iterator_ = Singleton<ChildProcessList>::get()->begin();
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
  return iterator_ == Singleton<ChildProcessList>::get()->end();
}
