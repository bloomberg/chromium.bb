// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_process/service_process_control.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

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
  SetChannel(IPC::ChannelProxy::Create(
      channel_id,
      IPC::Channel::MODE_NAMED_CLIENT,
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get()));
}

void ServiceProcessControl::SetChannel(scoped_ptr<IPC::ChannelProxy> channel) {
  channel_ = channel.Pass();
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
  if (launcher_.get())
    return;

  // If the service process is already running then connects to it.
  if (CheckServiceProcessReady()) {
    ConnectInternal();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents", SERVICE_EVENT_LAUNCH,
                            SERVICE_EVENT_MAX);

  scoped_ptr<base::CommandLine> cmd_line(CreateServiceProcessCommandLine());
  // And then start the process asynchronously.
  launcher_ = new Launcher(this, cmd_line.Pass());
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
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                              SERVICE_EVENT_LAUNCHED, SERVICE_EVENT_MAX);
    // After we have successfully created the service process we try to connect
    // to it. The launch task is transfered to a connect task.
    ConnectInternal();
  } else {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                              SERVICE_EVENT_LAUNCH_FAILED, SERVICE_EVENT_MAX);
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
    IPC_MESSAGE_HANDLER(ServiceHostMsg_Histograms, OnHistograms)
    IPC_MESSAGE_HANDLER(ServiceHostMsg_Printers, OnPrinters)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceProcessControl::OnChannelConnected(int32 peer_pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_CHANNEL_CONNECTED, SERVICE_EVENT_MAX);

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

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_CHANNEL_ERROR, SERVICE_EVENT_MAX);

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
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_INFO_REPLY, SERVICE_EVENT_MAX);
  if (!cloud_print_info_callback_.is_null()) {
    cloud_print_info_callback_.Run(proxy_info);
    cloud_print_info_callback_.Reset();
  }
}

void ServiceProcessControl::OnHistograms(
    const std::vector<std::string>& pickled_histograms) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_HISTOGRAMS_REPLY, SERVICE_EVENT_MAX);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::HistogramDeltaSerialization::DeserializeAndAddSamples(
      pickled_histograms);
  RunHistogramsCallback();
}

void ServiceProcessControl::RunHistogramsCallback() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!histograms_callback_.is_null()) {
    histograms_callback_.Run();
    histograms_callback_.Reset();
  }
  histograms_timeout_callback_.Cancel();
}

void ServiceProcessControl::OnPrinters(
    const std::vector<std::string>& printers) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION(
      "CloudPrint.ServiceEvents", SERVICE_PRINTERS_REPLY, SERVICE_EVENT_MAX);
  UMA_HISTOGRAM_COUNTS_10000("CloudPrint.AvailablePrinters", printers.size());
  if (!printers_callback_.is_null()) {
    printers_callback_.Run(printers);
    printers_callback_.Reset();
  }
}

bool ServiceProcessControl::GetCloudPrintProxyInfo(
    const CloudPrintProxyInfoCallback& cloud_print_info_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!cloud_print_info_callback.is_null());
  cloud_print_info_callback_.Reset();
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_INFO_REQUEST, SERVICE_EVENT_MAX);
  if (!Send(new ServiceMsg_GetCloudPrintProxyInfo()))
    return false;
  cloud_print_info_callback_ = cloud_print_info_callback;
  return true;
}

bool ServiceProcessControl::GetHistograms(
    const base::Closure& histograms_callback,
    const base::TimeDelta& timeout) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!histograms_callback.is_null());
  histograms_callback_.Reset();

  // If the service process is already running then connect to it.
  if (!CheckServiceProcessReady())
    return false;
  ConnectInternal();

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            SERVICE_EVENT_HISTOGRAMS_REQUEST,
                            SERVICE_EVENT_MAX);

  if (!Send(new ServiceMsg_GetHistograms()))
    return false;

  // Run timeout task to make sure |histograms_callback| is called.
  histograms_timeout_callback_.Reset(
      base::Bind(&ServiceProcessControl::RunHistogramsCallback,
                 base::Unretained(this)));
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
                                 histograms_timeout_callback_.callback(),
                                 timeout);

  histograms_callback_ = histograms_callback;
  return true;
}

bool ServiceProcessControl::GetPrinters(
    const PrintersCallback& printers_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!printers_callback.is_null());
  printers_callback_.Reset();
  UMA_HISTOGRAM_ENUMERATION(
      "CloudPrint.ServiceEvents", SERVICE_PRINTERS_REQUEST, SERVICE_EVENT_MAX);
  if (!Send(new ServiceMsg_GetPrinters()))
    return false;
  printers_callback_ = printers_callback;
  return true;
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

ServiceProcessControl::Launcher::Launcher(
    ServiceProcessControl* process,
    scoped_ptr<base::CommandLine> cmd_line)
    : process_(process),
      cmd_line_(cmd_line.Pass()),
      launched_(false),
      retry_count_(0),
      process_handle_(base::kNullProcessHandle) {
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

ServiceProcessControl::Launcher::~Launcher() {
  CloseProcessHandle();
}


void ServiceProcessControl::Launcher::Notify() {
  DCHECK(!notify_task_.is_null());
  notify_task_.Run();
  notify_task_.Reset();
}

void ServiceProcessControl::Launcher::CloseProcessHandle() {
  if (process_handle_ != base::kNullProcessHandle) {
    base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
  }
}

#if !defined(OS_MACOSX)
void ServiceProcessControl::Launcher::DoDetectLaunched() {
  DCHECK(!notify_task_.is_null());

  const uint32 kMaxLaunchDetectRetries = 10;
  launched_ = CheckServiceProcessReady();

  int exit_code = 0;
  if (launched_ || (retry_count_ >= kMaxLaunchDetectRetries) ||
      base::WaitForExitCodeWithTimeout(process_handle_, &exit_code,
                                       base::TimeDelta())) {
    CloseProcessHandle();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(&Launcher::Notify, this));
    return;
  }
  retry_count_++;

  // If the service process is not launched yet then check again in 2 seconds.
  const base::TimeDelta kDetectLaunchRetry = base::TimeDelta::FromSeconds(2);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&Launcher::DoDetectLaunched, this),
      kDetectLaunchRetry);
}

void ServiceProcessControl::Launcher::DoRun() {
  DCHECK(!notify_task_.is_null());

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  if (base::LaunchProcess(*cmd_line_, options, &process_handle_)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&Launcher::DoDetectLaunched, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(&Launcher::Notify, this));
  }
}
#endif  // !OS_MACOSX
