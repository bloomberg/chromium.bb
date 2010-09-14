// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_process_type.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_ipc_server.h"
#include "net/base/network_change_notifier.h"

#if defined(ENABLE_REMOTING)
#include "remoting/base/constants.h"
#include "remoting/base/encoder_zlib.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/json_host_config.h"

#if defined(OS_WIN)
#include "remoting/host/capturer_gdi.h"
#include "remoting/host/event_executor_win.h"
#elif defined(OS_LINUX)
#include "remoting/host/capturer_fake.h"
#include "remoting/host/event_executor_linux.h"
#elif defined(OS_MACOSX)
#include "remoting/host/capturer_mac.h"
#include "remoting/host/event_executor_mac.h"
#endif
#endif  // defined(ENABLED_REMOTING)

ServiceProcess* g_service_process = NULL;

// Delay in millseconds after the last service is disabled before we attempt
// a shutdown.
static const int64 kShutdownDelay = 60000;

ServiceProcess::ServiceProcess()
  : shutdown_event_(true, false),
    main_message_loop_(NULL),
    enabled_services_(0) {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize(MessageLoop* message_loop,
                                const CommandLine& command_line) {
  main_message_loop_ = message_loop;
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("ServiceProcess_IO"));
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
  service_prefs_.reset(new JsonPrefStore(pref_path,
                                         file_thread_->message_loop_proxy()));
  service_prefs_->ReadPrefs();

  DictionaryValue* values = service_prefs_->prefs();
  bool remoting_host_enabled = false;

  // Check if remoting host is already enabled.
  if (values->GetBoolean(prefs::kRemotingHostEnabled, &remoting_host_enabled) &&
      remoting_host_enabled) {
    // If true then we start the host.
    StartChromotingHost();
  }
  // Enable Cloud Print if needed. First check the command-line.
  bool cloud_print_proxy_enabled =
      command_line.HasSwitch(switches::kEnableCloudPrintProxy);
  if (!cloud_print_proxy_enabled) {
    // Then check if the cloud print proxy was previously enabled.
    values->GetBoolean(prefs::kCloudPrintProxyEnabled,
                       &cloud_print_proxy_enabled);
  }

  if (cloud_print_proxy_enabled) {
    GetCloudPrintProxy()->EnableForUser(lsid);
  }

  LOG(INFO) << "Starting Service Process IPC Server";
  ipc_server_.reset(new ServiceIPCServer(GetServiceProcessChannelName()));
  ipc_server_->Init();

  // After the IPC server has started we signal that the service process is
  // running.
  SignalServiceProcessRunning(kServiceProcessCloudPrint);
  return true;
}

bool ServiceProcess::Teardown() {
  if (service_prefs_.get()) {
    service_prefs_->WritePrefs();
    service_prefs_.reset();
  }
  cloud_print_proxy_.reset();

#if defined(ENABLE_REMOTING)
  ShutdownChromotingHost();
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

  // Delete the service process lock file when it shuts down.
  SignalServiceProcessStopped(kServiceProcessCloudPrint);
  return true;
}

void ServiceProcess::Shutdown() {
  // Quit the main message loop.
  main_message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

CloudPrintProxy* ServiceProcess::GetCloudPrintProxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new CloudPrintProxy());
    cloud_print_proxy_->Initialize(service_prefs_.get(), this);
  }
  return cloud_print_proxy_.get();
}

void ServiceProcess::OnCloudPrintProxyEnabled() {
  // Save the preference that we have enabled the cloud print proxy.
  service_prefs_->prefs()->SetBoolean(prefs::kCloudPrintProxyEnabled, true);
  service_prefs_->WritePrefs();
  OnServiceEnabled();
}

void ServiceProcess::OnCloudPrintProxyDisabled() {
  // Save the preference that we have disabled the cloud print proxy.
  service_prefs_->prefs()->SetBoolean(prefs::kCloudPrintProxyEnabled, false);
  service_prefs_->WritePrefs();
  OnServiceDisabled();
}

void ServiceProcess::OnServiceEnabled() {
  enabled_services_++;
  if (1 == enabled_services_) {
    AddServiceProcessToAutoStart();
  }
}

void ServiceProcess::OnServiceDisabled() {
  DCHECK(0 != enabled_services_);
  enabled_services_--;
  if (0 == enabled_services_) {
    RemoveServiceProcessFromAutoStart();
    // We will wait for some time to respond to IPCs before shutting down.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &ServiceProcess::ShutdownIfNoServices),
        kShutdownDelay);
  }
}

void ServiceProcess::ShutdownIfNoServices() {
  if (0 == enabled_services_) {
    Shutdown();
  }
}

bool ServiceProcess::AddServiceProcessToAutoStart() {
// TODO(sanjeevr): This needs to move to some common place like base or
// chrome/common and implementation for non-Windows platforms needs to be added.
#if defined(OS_WIN)
  FilePath chrome_path;
  if (PathService::Get(base::FILE_EXE, &chrome_path)) {
    CommandLine cmd_line(chrome_path);
    cmd_line.AppendSwitchASCII(switches::kProcessType,
                               switches::kServiceProcess);
    // We need a unique name for the command per user-date-dir. Just use the
    // channel name.
    return win_util::AddCommandToAutoRun(
        HKEY_CURRENT_USER, UTF8ToWide(GetServiceProcessChannelName()),
        cmd_line.command_line_string());
  }
#endif  // defined(OS_WIN)
  return false;
}

bool ServiceProcess::RemoveServiceProcessFromAutoStart() {
// TODO(sanjeevr): This needs to move to some common place like base or
// chrome/common and implementation for non-Windows platforms needs to be added.
#if defined(OS_WIN)
  return win_util::RemoveCommandFromAutoRun(
      HKEY_CURRENT_USER, UTF8ToWide(GetServiceProcessChannelName()));
#endif  // defined(OS_WIN)
  return false;
}


#if defined(ENABLE_REMOTING)
bool ServiceProcess::EnableChromotingHostWithTokens(
    const std::string& login,
    const std::string& remoting_token,
    const std::string& talk_token) {
  // Save the login info and tokens.
  remoting_login_ = login;
  remoting_token_ = remoting_token;
  talk_token_ = talk_token;

  // Use the remoting directory to register the host.
  if (remoting_directory_.get())
    remoting_directory_->CancelRequest();
  remoting_directory_.reset(new RemotingDirectoryService(this));
  remoting_directory_->AddHost(remoting_token);
  return true;
}

bool ServiceProcess::StartChromotingHost() {
  // We have already started.
  if (chromoting_context_.get())
    return true;

  // Load chromoting config from the disk.
  LoadChromotingConfig();

  // Start the chromoting context first.
  chromoting_context_.reset(new remoting::ChromotingHostContext());
  chromoting_context_->Start();

  // Create capturer, encoder and executor. The ownership will be transfered
  // to the chromoting host.
  scoped_ptr<remoting::Capturer> capturer;
  scoped_ptr<remoting::Encoder> encoder;
  scoped_ptr<remoting::EventExecutor> executor;

#if defined(OS_WIN)
  capturer.reset(new remoting::CapturerGdi());
  executor.reset(new remoting::EventExecutorWin(capturer.get()));
#elif defined(OS_LINUX)
  capturer.reset(new remoting::CapturerFake());
  executor.reset(new remoting::EventExecutorLinux(capturer.get()));
#elif defined(OS_MACOSX)
  capturer.reset(new remoting::CapturerMac());
  executor.reset(new remoting::EventExecutorMac(capturer.get()));
#endif
  encoder.reset(new remoting::EncoderZlib());

  // Create a chromoting host object.
  chromoting_host_ = new remoting::ChromotingHost(chromoting_context_.get(),
                                                  chromoting_config_,
                                                  capturer.release(),
                                                  encoder.release(),
                                                  executor.release());

  // Then start the chromoting host.
  // When ChromotingHost is shutdown because of failure or a request that
  // we made OnChromotingShutdown() is calls.
  chromoting_host_->Start(
      NewRunnableMethod(this, &ServiceProcess::OnChromotingHostShutdown));
  OnServiceEnabled();
  return true;
}

bool ServiceProcess::ShutdownChromotingHost() {
  // Chromoting host doesn't exist so return true.
  if (!chromoting_host_)
    return true;

  // Shutdown the chromoting host asynchronously. This will signal the host to
  // shutdown, we'll actually wait for all threads to stop when we destroy
  // the chromoting context.
  chromoting_host_->Shutdown();
  chromoting_host_ = NULL;
  return true;
}

void ServiceProcess::OnRemotingHostAdded() {
  // Save configuration for chromoting.
  SaveChromotingConfig(remoting_login_,
                       talk_token_,
                       remoting_directory_->host_id(),
                       remoting_directory_->host_name(),
                       remoting_directory_->host_key_pair());
  remoting_directory_.reset();
  remoting_login_ = "";
  remoting_token_ = "";
  talk_token_ = "";

  // Save the preference that we have enabled the remoting host.
  service_prefs_->prefs()->SetBoolean(prefs::kRemotingHostEnabled, true);

  // Force writing prefs to the disk.
  service_prefs_->WritePrefs();

  // TODO(hclam): If we have a problem we need to send an IPC message back
  // to the client that started this.
  bool ret = StartChromotingHost();
  DCHECK(ret);
}

void ServiceProcess::OnRemotingDirectoryError() {
  remoting_directory_.reset();
  remoting_login_ = "";
  remoting_token_ = "";
  talk_token_ = "";

  // TODO(hclam): If we have a problem we need to send an IPC message back
  // to the client that started this.
}

// A util function to update the login information to host config.
static void SaveChromotingConfigFunc(remoting::JsonHostConfig* config,
                                     const std::string& login,
                                     const std::string& token,
                                     const std::string& host_id,
                                     const std::string& host_name) {
  config->SetString(remoting::kXmppLoginConfigPath, login);
  config->SetString(remoting::kXmppAuthTokenConfigPath, token);
  config->SetString(remoting::kHostIdConfigPath, host_id);
  config->SetString(remoting::kHostNameConfigPath, host_name);
}

void ServiceProcess::SaveChromotingConfig(
    const std::string& login,
    const std::string& token,
    const std::string& host_id,
    const std::string& host_name,
    remoting::HostKeyPair* host_key_pair) {
  // First we need to load the config first.
  LoadChromotingConfig();

  // And then do the update.
  chromoting_config_->Update(
      NewRunnableFunction(&SaveChromotingConfigFunc, chromoting_config_.get(),
                          login, token, host_id, host_name));

  // And then save the key pair.
  host_key_pair->Save(chromoting_config_);
}

void ServiceProcess::LoadChromotingConfig() {
  // TODO(hclam): We really should be doing this on IO thread so we are not
  // blocked on file IOs.
  if (chromoting_config_)
    return;

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath chromoting_config_path =
      user_data_dir.Append(FILE_PATH_LITERAL(".ChromotingConfig.json"));
  chromoting_config_ = new remoting::JsonHostConfig(
      chromoting_config_path, file_thread_->message_loop_proxy());
  if (!chromoting_config_->Read()) {
    LOG(INFO) << "Failed to read chromoting config file.";
  }
}

void ServiceProcess::OnChromotingHostShutdown() {
  // TODO(hclam): Implement.
}
#endif

ServiceProcess::~ServiceProcess() {
  Teardown();
  g_service_process = NULL;
}

// Disable refcounting for runnable method because it is really not needed
// when we post tasks on the main message loop.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ServiceProcess);
