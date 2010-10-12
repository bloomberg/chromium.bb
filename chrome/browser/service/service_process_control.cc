// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service/service_process_control.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_util.h"

// ServiceProcessControl::Launcher implementation.
// This class is responsible for launching the service process on the
// PROCESS_LAUNCHER thread.
class ServiceProcessControl::Launcher
    : public base::RefCountedThreadSafe<ServiceProcessControl::Launcher> {
 public:
  Launcher(ServiceProcessControl* process, CommandLine* cmd_line)
      : process_(process),
        cmd_line_(cmd_line),
        launched_(false),
        retry_count_(0) {
  }

  // Execute the command line to start the process asynchronously.
  // After the comamnd is executed |task| is called with the process handle on
  // the UI thread.
  void Run(Task* task) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
                           NewRunnableMethod(this, &Launcher::DoRun, task));
  }

  bool launched() const { return launched_; }

 private:
  void DoRun(Task* task) {
    base::LaunchApp(*cmd_line_.get(), false, true, NULL);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &Launcher::DoDetectLaunched, task));
  }

  void DoDetectLaunched(Task* task) {
    const uint32 kMaxLaunchDetectRetries = 10;

    launched_ = CheckServiceProcessReady();
    if (launched_ || (retry_count_ >= kMaxLaunchDetectRetries)) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &Launcher::Notify, task));
      return;
    }
    retry_count_++;
    // If the service process is not launched yet then check again in 2 seconds.
    const int kDetectLaunchRetry = 2000;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &Launcher::DoDetectLaunched, task),
        kDetectLaunchRetry);
  }

  void Notify(Task* task) {
    task->Run();
    delete task;
  }

  ServiceProcessControl* process_;
  scoped_ptr<CommandLine> cmd_line_;
  bool launched_;
  uint32 retry_count_;
};

// ServiceProcessControl implementation.
ServiceProcessControl::ServiceProcessControl(Profile* profile)
    : profile_(profile),
      message_handler_(NULL) {
}

ServiceProcessControl::~ServiceProcessControl() {
}

void ServiceProcessControl::ConnectInternal(Task* task) {
  // If the channel has already been established then we run the task
  // and return.
  if (channel_.get()) {
    if (task) {
      task->Run();
      delete task;
    }
    return;
  }

  // Actually going to connect.
  LOG(INFO) << "Connecting to Service Process IPC Server";
  connect_done_task_.reset(task);

  // Run the IPC channel on the shared IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();

  // TODO(hclam): Handle error connecting to channel.
  const std::string channel_id = GetServiceProcessChannelName();
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_CLIENT, this, NULL,
                           io_thread->message_loop(), true,
                           g_browser_process->shutdown_event()));
  channel_->set_sync_messages_with_no_timeout_allowed(false);

  // We just established a channel with the service process. Notify it if an
  // upgrade is available.
  if (Singleton<UpgradeDetector>::get()->notify_upgrade()) {
    Send(new ServiceMsg_UpdateAvailable);
  } else {
    if (registrar_.IsEmpty())
      registrar_.Add(this, NotificationType::UPGRADE_RECOMMENDED,
                     NotificationService::AllSources());
  }
}

void ServiceProcessControl::Launch(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the service process is already running then connects to it.
  if (CheckServiceProcessReady()) {
    ConnectInternal(task);
    return;
  }

  // A service process should have a different mechanism for starting, but now
  // we start it as if it is a child process.
  FilePath exe_path = ChildProcessHost::GetChildPath(true);
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get service process binary name.";
  }

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kServiceProcess);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  FilePath user_data_dir =
      browser_command_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    cmd_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  std::string logging_level = browser_command_line.GetSwitchValueASCII(
      switches::kLoggingLevel);
  if (!logging_level.empty())
    cmd_line->AppendSwitchASCII(switches::kLoggingLevel, logging_level);

  if (browser_command_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    cmd_line->AppendSwitch(switches::kWaitForDebugger);
  }

  // And then start the process asynchronously.
  launcher_ = new Launcher(this, cmd_line);
  launcher_->Run(
      NewRunnableMethod(this, &ServiceProcessControl::OnProcessLaunched, task));
}

void ServiceProcessControl::OnProcessLaunched(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (launcher_->launched()) {
    // After we have successfully created the service process we try to connect
    // to it. The launch task is transfered to a connect task.
    ConnectInternal(task);
  } else if (task) {
    // If we don't have process handle that means launching the service process
    // has failed.
    task->Run();
    delete task;
  }

  // We don't need the launcher anymore.
  launcher_ = NULL;
}

void ServiceProcessControl::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ServiceProcessControl, message)
      IPC_MESSAGE_HANDLER(ServiceHostMsg_GoodDay, OnGoodDay)
      IPC_MESSAGE_HANDLER(ServiceHostMsg_CloudPrintProxy_IsEnabled,
                          OnCloudPrintProxyIsEnabled)
  IPC_END_MESSAGE_MAP()
}

void ServiceProcessControl::OnChannelConnected(int32 peer_pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!connect_done_task_.get())
    return;
  connect_done_task_->Run();
  connect_done_task_.reset();
}

void ServiceProcessControl::OnChannelError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  channel_.reset();
  if (!connect_done_task_.get())
    return;
  connect_done_task_->Run();
  connect_done_task_.reset();
}

bool ServiceProcessControl::Send(IPC::Message* message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!channel_.get())
    return false;
  return channel_->Send(message);
}

// NotificationObserver implementation.
void ServiceProcessControl::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::UPGRADE_RECOMMENDED) {
    Send(new ServiceMsg_UpdateAvailable);
  }
}


void ServiceProcessControl::OnGoodDay() {
  if (!message_handler_)
    return;

  message_handler_->OnGoodDay();
}

void ServiceProcessControl::OnCloudPrintProxyIsEnabled(bool enabled,
                                                       std::string email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cloud_print_status_callback_ != NULL) {
    cloud_print_status_callback_->Run(enabled, email);
    cloud_print_status_callback_.reset();
  }
}

bool ServiceProcessControl::SendHello() {
  return Send(new ServiceMsg_Hello());
}

bool ServiceProcessControl::Shutdown() {
  bool ret = Send(new ServiceMsg_Shutdown());
  channel_.reset();
  return ret;
}

bool ServiceProcessControl::EnableRemotingWithTokens(
    const std::string& user,
    const std::string& remoting_token,
    const std::string& talk_token) {
  return Send(
      new ServiceMsg_EnableRemotingWithTokens(user, remoting_token,
                                              talk_token));
}

bool ServiceProcessControl::GetCloudPrintProxyStatus(
    Callback2<bool, std::string>::Type* cloud_print_status_callback) {
  DCHECK(cloud_print_status_callback);
  cloud_print_status_callback_.reset(cloud_print_status_callback);
  return Send(new ServiceMsg_IsCloudPrintProxyEnabled);
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(ServiceProcessControl);
