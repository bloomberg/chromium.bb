// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_ipc_server.h"
#include "chrome/service/service_process_prefs.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

#if defined(ENABLE_REMOTING)
#include "chrome/service/remoting/chromoting_host_manager.h"
#endif  // defined(ENABLED_REMOTING)

ServiceProcess* g_service_process = NULL;

namespace {

// Delay in millseconds after the last service is disabled before we attempt
// a shutdown.
const int64 kShutdownDelay = 60000;

const char kDefaultServiceProcessLocale[] = "en-US";

class ServiceIOThread : public base::Thread {
 public:
  explicit ServiceIOThread(const char* name);
  virtual ~ServiceIOThread();

 protected:
  virtual void CleanUp();

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceIOThread);
};

ServiceIOThread::ServiceIOThread(const char* name) : base::Thread(name) {}
ServiceIOThread::~ServiceIOThread() {
  // We cannot rely on our base class to stop the thread since we want our
  // CleanUp function to run.
  Stop();
}

void ServiceIOThread::CleanUp() {
  URLFetcher::CancelAll();
}

}  // namespace

ServiceProcess::ServiceProcess()
  : shutdown_event_(true, false),
    main_message_loop_(NULL),
    enabled_services_(0),
    update_available_(false) {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize(MessageLoop* message_loop,
                                const CommandLine& command_line) {
  main_message_loop_ = message_loop;
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.reset(new ServiceIOThread("ServiceProcess_IO"));
  file_thread_.reset(new base::Thread("ServiceProcess_File"));
  if (!io_thread_->StartWithOptions(options) ||
      !file_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Teardown();
    return false;
  }

  // See if we have been suppiled an LSID in the command line. This LSID will
  // override the credentials we use for Cloud Print.
  std::string lsid = command_line.GetSwitchValueASCII(
          switches::kServiceAccountLsid);

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath pref_path = user_data_dir.Append(chrome::kServiceStateFileName);
  service_prefs_.reset(
      new ServiceProcessPrefs(pref_path, file_thread_->message_loop_proxy()));
  service_prefs_->ReadPrefs();

  // Check if a locale override has been specified on the command-line.
  std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
  if (!locale.empty()) {
    service_prefs_->SetString(prefs::kApplicationLocale, locale);
    service_prefs_->WritePrefs();
  } else {
    // If no command-line value was specified, read the last used locale from
    // the prefs.
    service_prefs_->GetString(prefs::kApplicationLocale, &locale);
    // If no locale was specified anywhere, use the default one.
    if (locale.empty())
      locale = kDefaultServiceProcessLocale;
  }
  ResourceBundle::InitSharedInstance(locale);

#if defined(ENABLE_REMOTING)
  // Initialize chromoting host manager.
  remoting_host_manager_ = new remoting::ChromotingHostManager(this);
  remoting_host_manager_->Initialize(file_thread_->message_loop_proxy());
#endif

  // Enable Cloud Print if needed. First check the command-line.
  bool cloud_print_proxy_enabled =
      command_line.HasSwitch(switches::kEnableCloudPrintProxy);
  if (!cloud_print_proxy_enabled) {
    // Then check if the cloud print proxy was previously enabled.
    service_prefs_->GetBoolean(prefs::kCloudPrintProxyEnabled,
                               &cloud_print_proxy_enabled);
  }

  if (cloud_print_proxy_enabled) {
    GetCloudPrintProxy()->EnableForUser(lsid);
  }

  VLOG(1) << "Starting Service Process IPC Server";
  ipc_server_.reset(new ServiceIPCServer(GetServiceProcessChannelName()));
  ipc_server_->Init();

  // After the IPC server has started we signal that the service process is
  // ready.
  if (!ServiceProcessState::GetInstance()->SignalReady(
          io_thread_->message_loop_proxy(),
          NewRunnableMethod(this, &ServiceProcess::Shutdown))) {
    return false;
  }

  // See if we need to stay running.
  ScheduleShutdownCheck();
  return true;
}

bool ServiceProcess::Teardown() {
  service_prefs_.reset();
  cloud_print_proxy_.reset();

#if defined(ENABLE_REMOTING)
  remoting_host_manager_->Teardown();
#endif

  ipc_server_.reset();
  // Signal this event before shutting down the service process. That way all
  // background threads can cleanup.
  shutdown_event_.Signal();
  io_thread_.reset();
  file_thread_.reset();
  // The NetworkChangeNotifier must be destroyed after all other threads that
  // might use it have been shut down.
  network_change_notifier_.reset();

  ServiceProcessState::GetInstance()->SignalStopped();
  return true;
}

void ServiceProcess::Shutdown() {
  // Quit the main message loop.
  main_message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

bool ServiceProcess::HandleClientDisconnect() {
  // If there are no enabled services or if there is an update available
  // we want to shutdown right away. Else we want to keep listening for
  // new connections.
  if (!enabled_services_ || update_available()) {
    Shutdown();
    return false;
  }
  return true;
}

CloudPrintProxy* ServiceProcess::GetCloudPrintProxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new CloudPrintProxy());
    cloud_print_proxy_->Initialize(service_prefs_.get(), this);
  }
  return cloud_print_proxy_.get();
}

void ServiceProcess::OnCloudPrintProxyEnabled(bool persist_state) {
  if (persist_state) {
    // Save the preference that we have enabled the cloud print proxy.
    service_prefs_->SetBoolean(prefs::kCloudPrintProxyEnabled, true);
    service_prefs_->WritePrefs();
  }
  OnServiceEnabled();
}

void ServiceProcess::OnCloudPrintProxyDisabled(bool persist_state) {
  if (persist_state) {
    // Save the preference that we have disabled the cloud print proxy.
    service_prefs_->SetBoolean(prefs::kCloudPrintProxyEnabled, false);
    service_prefs_->WritePrefs();
  }
  OnServiceDisabled();
}

void ServiceProcess::OnRemotingHostEnabled() {
  OnServiceEnabled();
}

void ServiceProcess::OnRemotingHostDisabled() {
  OnServiceDisabled();
}

void ServiceProcess::OnServiceEnabled() {
  enabled_services_++;
  if (1 == enabled_services_) {
    ServiceProcessState::GetInstance()->AddToAutoRun();
  }
}

void ServiceProcess::OnServiceDisabled() {
  DCHECK_NE(enabled_services_, 0);
  enabled_services_--;
  if (0 == enabled_services_) {
    ServiceProcessState::GetInstance()->RemoveFromAutoRun();
    // We will wait for some time to respond to IPCs before shutting down.
    ScheduleShutdownCheck();
  }
}

void ServiceProcess::ScheduleShutdownCheck() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &ServiceProcess::ShutdownIfNeeded),
      kShutdownDelay);
}

void ServiceProcess::ShutdownIfNeeded() {
  if (0 == enabled_services_) {
    if (ipc_server_->is_client_connected()) {
      // If there is a client connected, we need to try again later.
      // Note that there is still a timing window here because a client may
      // decide to connect at this point.
      // TODO(sanjeevr): Fix this timing window.
      ScheduleShutdownCheck();
    } else {
      Shutdown();
    }
  }
}

ServiceProcess::~ServiceProcess() {
  Teardown();
  g_service_process = NULL;
}

// Disable refcounting for runnable method because it is really not needed
// when we post tasks on the main message loop.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ServiceProcess);
