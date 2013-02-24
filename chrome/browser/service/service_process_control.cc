// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service/service_process_control.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/child_process_host.h"
#include "ui/base/ui_base_switches.h"

using content::BrowserThread;
using content::ChildProcessHost;

// ServiceProcessControl implementation.
ServiceProcessControl::ServiceProcessControl() {
}

ServiceProcessControl::~ServiceProcessControl() {
}

void ServiceProcessControl::ConnectInternal() {
  // If the channel has already been established then we run the task
  // and return.
  if (channel_.get()) {
    RunConnectDoneTasks();
    return;
  }

  // Actually going to connect.
  VLOG(1) << "Connecting to Service Process IPC Server";

  // TODO(hclam): Handle error connecting to channel.
  const IPC::ChannelHandle channel_id = GetServiceProcessChannel();
  SetChannel(new IPC::ChannelProxy(
      channel_id, IPC::Channel::MODE_NAMED_CLIENT, this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
}

void ServiceProcessControl::SetChannel(IPC::ChannelProxy* channel) {
  channel_.reset(channel);
}

void ServiceProcessControl::RunConnectDoneTasks() {
  // The tasks executed here may add more tasks to the vector. So copy
  // them to the stack before executing them. This way recursion is
  // avoided.
  TaskList tasks;

  if (IsConnected()) {
    tasks.swap(connect_success_tasks_);
    RunAllTasksHelper(&tasks);
    DCHECK(tasks.empty());
    connect_failure_tasks_.clear();
  } else {
    tasks.swap(connect_failure_tasks_);
    RunAllTasksHelper(&tasks);
    DCHECK(tasks.empty());
    connect_success_tasks_.clear();
  }
}

// static
void ServiceProcessControl::RunAllTasksHelper(TaskList* task_list) {
  TaskList::iterator index = task_list->begin();
  while (index != task_list->end()) {
    (*index).Run();
    index = task_list->erase(index);
  }
}

bool ServiceProcessControl::IsConnected() const {
  return channel_ != NULL;
}

void ServiceProcessControl::Launch(const base::Closure& success_task,
                                   const base::Closure& failure_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Closure failure = failure_task;
  if (!success_task.is_null())
    connect_success_tasks_.push_back(success_task);

  if (!failure.is_null())
    connect_failure_tasks_.push_back(failure);

  // If we already in the process of launching, then we are done.
  if (launcher_)
    return;

  // If the service process is already running then connects to it.
  if (CheckServiceProcessReady()) {
    ConnectInternal();
    return;
  }

  // A service process should have a different mechanism for starting, but now
  // we start it as if it is a child process.

#if defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    NOTREACHED() << "Unable to get service process binary name.";

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kServiceProcess);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  base::FilePath user_data_dir =
      browser_command_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    cmd_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  std::string logging_level = browser_command_line.GetSwitchValueASCII(
      switches::kLoggingLevel);
  if (!logging_level.empty())
    cmd_line->AppendSwitchASCII(switches::kLoggingLevel, logging_level);

  std::string v_level = browser_command_line.GetSwitchValueASCII(
      switches::kV);
  if (!v_level.empty())
    cmd_line->AppendSwitchASCII(switches::kV, v_level);

  std::string v_modules = browser_command_line.GetSwitchValueASCII(
      switches::kVModule);
  if (!v_modules.empty())
    cmd_line->AppendSwitchASCII(switches::kVModule, v_modules);

  if (browser_command_line.HasSwitch(switches::kWaitForDebuggerChildren))
    cmd_line->AppendSwitch(switches::kWaitForDebugger);

  if (browser_command_line.HasSwitch(switches::kEnableLogging))
    cmd_line->AppendSwitch(switches::kEnableLogging);

  std::string locale = g_browser_process->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  // And then start the process asynchronously.
  launcher_ = new Launcher(this, cmd_line);
  launcher_->Run(base::Bind(&ServiceProcessControl::OnProcessLaunched,
                            base::Unretained(this)));
}

void ServiceProcessControl::Disconnect() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  channel_.reset();
}

void ServiceProcessControl::OnProcessLaunched() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (launcher_->launched()) {
    // After we have successfully created the service process we try to connect
    // to it. The launch task is transfered to a connect task.
    ConnectInternal();
  } else {
    // If we don't have process handle that means launching the service process
    // has failed.
    RunConnectDoneTasks();
  }

  // We don't need the launcher anymore.
  launcher_ = NULL;
}

bool ServiceProcessControl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceProcessControl, message)
    IPC_MESSAGE_HANDLER(ServiceHostMsg_CloudPrintProxy_Info,
                        OnCloudPrintProxyInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceProcessControl::OnChannelConnected(int32 peer_pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We just established a channel with the service process. Notify it if an
  // upgrade is available.
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    Send(new ServiceMsg_UpdateAvailable);
  } else {
    if (registrar_.IsEmpty())
      registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                     content::NotificationService::AllSources());
  }
  RunConnectDoneTasks();
}

void ServiceProcessControl::OnChannelError() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  channel_.reset();
  RunConnectDoneTasks();
}

bool ServiceProcessControl::Send(IPC::Message* message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!channel_.get())
    return false;
  return channel_->Send(message);
}

// content::NotificationObserver implementation.
void ServiceProcessControl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_UPGRADE_RECOMMENDED) {
    Send(new ServiceMsg_UpdateAvailable);
  }
}

void ServiceProcessControl::OnCloudPrintProxyInfo(
    const cloud_print::CloudPrintProxyInfo& proxy_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!cloud_print_info_callback_.is_null()) {
    cloud_print_info_callback_.Run(proxy_info);
    cloud_print_info_callback_.Reset();
  }
}

bool ServiceProcessControl::GetCloudPrintProxyInfo(
    const CloudPrintProxyInfoHandler& cloud_print_info_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(false, cloud_print_info_callback.is_null());

  cloud_print_info_callback_ = cloud_print_info_callback;
  return Send(new ServiceMsg_GetCloudPrintProxyInfo());
}

bool ServiceProcessControl::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool ret = Send(new ServiceMsg_Shutdown());
  channel_.reset();
  return ret;
}

// static
ServiceProcessControl* ServiceProcessControl::GetInstance() {
  return Singleton<ServiceProcessControl>::get();
}

ServiceProcessControl::Launcher::Launcher(ServiceProcessControl* process,
                                          CommandLine* cmd_line)
    : process_(process),
      cmd_line_(cmd_line),
      launched_(false),
      retry_count_(0) {
}

// Execute the command line to start the process asynchronously.
// After the command is executed, |task| is called with the process handle on
// the UI thread.
void ServiceProcessControl::Launcher::Run(const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notify_task_ = task;
  BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
                          base::Bind(&Launcher::DoRun, this));
}

ServiceProcessControl::Launcher::~Launcher() {}

void ServiceProcessControl::Launcher::Notify() {
  DCHECK_EQ(false, notify_task_.is_null());
  notify_task_.Run();
  notify_task_.Reset();
}

#if !defined(OS_MACOSX)
void ServiceProcessControl::Launcher::DoDetectLaunched() {
  DCHECK_EQ(false, notify_task_.is_null());

  const uint32 kMaxLaunchDetectRetries = 10;
  launched_ = CheckServiceProcessReady();
  if (launched_ || (retry_count_ >= kMaxLaunchDetectRetries)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(&Launcher::Notify, this));
    return;
  }
  retry_count_++;

  // If the service process is not launched yet then check again in 2 seconds.
  const base::TimeDelta kDetectLaunchRetry = base::TimeDelta::FromSeconds(2);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&Launcher::DoDetectLaunched, this),
      kDetectLaunchRetry);
}

void ServiceProcessControl::Launcher::DoRun() {
  DCHECK_EQ(false, notify_task_.is_null());

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  if (base::LaunchProcess(*cmd_line_, options, NULL)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&Launcher::DoDetectLaunched, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(&Launcher::Notify, this));
  }
}
#endif  // !OS_MACOSX
