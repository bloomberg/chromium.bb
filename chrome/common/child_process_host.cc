// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_host.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/google_update_settings.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"

// This is defined in chrome/browser/google_update_settings_linux.cc.  It's the
// static string containing the user's unique GUID.  We send this in the crash
// report.
namespace google_update {
extern std::string linux_guid;
}  // namespace google_update
#endif  // OS_LINUX


namespace {

typedef std::list<ChildProcessHost*> ChildProcessList;

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


ChildProcessHost::ChildProcessHost(
    ProcessType type, ResourceDispatcherHost* resource_dispatcher_host)
    : Receiver(type, -1),
      ALLOW_THIS_IN_INITIALIZER_LIST(listener_(this)),
      resource_dispatcher_host_(resource_dispatcher_host),
      opening_channel_(false) {
  Singleton<ChildProcessList>::get()->push_back(this);
}


ChildProcessHost::~ChildProcessHost() {
  Singleton<ChildProcessList>::get()->remove(this);

  resource_dispatcher_host_->CancelRequestsForProcess(id());

  if (handle())
    ProcessWatcher::EnsureProcessTerminated(handle());
}

// static
FilePath ChildProcessHost::GetChildPath() {
  FilePath child_path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kBrowserSubprocessPath);
  if (!child_path.empty())
    return child_path;

  FilePath path;
  PathService::Get(base::FILE_EXE, &path);

#if !defined(OS_MACOSX)
  // On most platforms, the child executable is the same as the current
  // executable.
  return path;
#else
  // On the Mac, the child executable lives at a predefined location within
  // the current app bundle.

  // Figure out the current executable name.  In a browser, this will be
  // "Chromium" or "Google Chrome".  The child name will be the browser
  // executable name with " Helper" appended.  The child app bundle name will
  // be that name with ".app" appended.
  FilePath::StringType child_exe_name = path.BaseName().value();
  const FilePath::StringType child_suffix = FILE_PATH_LITERAL(" Helper");

  if (child_exe_name.size() > child_suffix.size()) {
    size_t test_suffix_pos = child_exe_name.size() - child_suffix.size();
    const FilePath::CharType* test_suffix =
        child_exe_name.c_str() + test_suffix_pos;
    if (strcmp(test_suffix, child_suffix.c_str()) == 0) {
      // FILE_EXE already ends with the child suffix and therefore already
      // refers to the child process path.  Just return it.
      return path;
    }
  }

  child_exe_name.append(child_suffix);
  FilePath::StringType child_app_name = child_exe_name;
  child_app_name.append(FILE_PATH_LITERAL(".app"));
  // The renderer app bundle lives in the browser app bundle's Resources
  // directory.  Take off the executable name.
  path = path.DirName();

  // Take off the MacOS component, after verifying that's what's there.
  FilePath::StringType macos = path.BaseName().value();
  DCHECK_EQ(macos, FILE_PATH_LITERAL("MacOS"));
  path = path.DirName();

  // Append the components to get to the sub-app bundle's executable.
  path = path.Append(FILE_PATH_LITERAL("Resources"));
  path = path.Append(child_app_name);
  path = path.Append(FILE_PATH_LITERAL("Contents"));
  path = path.Append(FILE_PATH_LITERAL("MacOS"));
  path = path.Append(child_exe_name);

  return path;
#endif  // OS_MACOSX
}

// static
void ChildProcessHost::SetCrashReporterCommandLine(CommandLine* command_line) {
#if defined(USE_LINUX_BREAKPAD)
  const bool unattended = (getenv("CHROME_HEADLESS") != NULL);
  if (unattended || GoogleUpdateSettings::GetCollectStatsConsent()) {
    command_line->AppendSwitchWithValue(switches::kEnableCrashReporter,
                                        ASCIIToWide(google_update::linux_guid +
                                                    "," +
                                                    base::GetLinuxDistro()));
  }
#elif defined(OS_MACOSX)
  if (GoogleUpdateSettings::GetCollectStatsConsent())
    command_line->AppendSwitch(switches::kEnableCrashReporter);
#endif  // OS_MACOSX
}

bool ChildProcessHost::CreateChannel() {
  channel_id_ = GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(
      channel_id_, IPC::Channel::MODE_SERVER, &listener_));
  if (!channel_->Connect())
    return false;

  opening_channel_ = true;

  return true;
}

void ChildProcessHost::SetHandle(base::ProcessHandle process) {
  DCHECK(!handle());
  set_handle(process);
}

void ChildProcessHost::InstanceCreated() {
  Notify(NotificationType::CHILD_INSTANCE_CREATED);
}

bool ChildProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void ChildProcessHost::Notify(NotificationType type) {
  resource_dispatcher_host_->ui_loop()->PostTask(
      FROM_HERE, new ChildNotificationTask(type, this));
}

void ChildProcessHost::OnChildDied() {
  DCHECK(handle());

  bool did_crash = base::DidProcessCrash(NULL, handle());
  if (did_crash) {
    // Report that this child process crashed.
    Notify(NotificationType::CHILD_PROCESS_CRASHED);
  }
  // Notify in the main loop of the disconnection.
  Notify(NotificationType::CHILD_PROCESS_HOST_DISCONNECTED);

  // On POSIX, once we've called DidProcessCrash, handle() is no longer
  // valid.  Ensure the destructor doesn't try to use it.
  set_handle(base::kNullProcessHandle);

  delete this;
}

ChildProcessHost::ListenerHook::ListenerHook(ChildProcessHost* host)
    : host_(host) {
}

void ChildProcessHost::ListenerHook::OnMessageReceived(
    const IPC::Message& msg) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::current();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool msg_is_ok = true;
  bool handled = host_->resource_dispatcher_host_->OnMessageReceived(
      msg, host_, &msg_is_ok);

  if (!handled) {
    if (msg.type() == PluginProcessHostMsg_ShutdownRequest::ID) {
      // Must remove the process from the list now, in case it gets used for a
      // new instance before our watcher tells us that the process terminated.
      Singleton<ChildProcessList>::get()->remove(host_);
      if (host_->CanShutdown())
        host_->Send(new PluginProcessMsg_Shutdown());
    } else {
      host_->OnMessageReceived(msg);
    }
  }

  if (!msg_is_ok)
    base::KillProcess(host_->handle(), ResultCodes::KILLED_BAD_MESSAGE, false);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, host_->channel_id_);
#endif
}

void ChildProcessHost::ListenerHook::OnChannelConnected(int32 peer_pid) {
  host_->opening_channel_ = false;
  host_->OnChannelConnected(peer_pid);

#if defined(IPC_MESSAGE_LOG_ENABLED)
  bool enabled = IPC::Logging::current()->Enabled();
  host_->Send(new PluginProcessMsg_SetIPCLoggingEnabled(enabled));
#endif

  host_->Send(new PluginProcessMsg_AskBeforeShutdown());

  // Notify in the main loop of the connection.
  host_->Notify(NotificationType::CHILD_PROCESS_HOST_CONNECTED);
}

void ChildProcessHost::ListenerHook::OnChannelError() {
  host_->opening_channel_ = false;
  host_->OnChannelError();

  // This will delete host_, which will also destroy this!
  host_->OnChildDied();
}


ChildProcessHost::Iterator::Iterator()
    : all_(true), type_(UNKNOWN_PROCESS) {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
  iterator_ = Singleton<ChildProcessList>::get()->begin();
}

ChildProcessHost::Iterator::Iterator(ProcessType type)
    : all_(false), type_(type) {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
  iterator_ = Singleton<ChildProcessList>::get()->begin();
  if (!Done() && (*iterator_)->type() != type_)
    ++(*this);
}

ChildProcessHost* ChildProcessHost::Iterator::operator++() {
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

bool ChildProcessHost::Iterator::Done() {
  return iterator_ == Singleton<ChildProcessList>::get()->end();
}
