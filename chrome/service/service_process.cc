// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <algorithm>

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
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

ServiceProcess::ServiceProcess()
  : shutdown_event_(true, false),
    main_message_loop_(NULL) {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize(MessageLoop* message_loop) {
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

  // TODO(hclam): Each type of service process should has it own instance of
  // process and thus channel, but now we have only one process for all types
  // so the type parameter doesn't matter now.
  LOG(INFO) << "Starting Service Process IPC Server";
  ipc_server_.reset(new ServiceIPCServer(
      GetServiceProcessChannelName(kServiceProcessCloudPrint)));
  ipc_server_->Init();

  // After the IPC server has started we can create the lock file to indicate
  // that we have started.
  bool ret = CreateServiceProcessLockFile(kServiceProcessCloudPrint);
  DCHECK(ret) << "Failed to create service process lock file.";
  return ret;
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
  bool ret = DeleteServiceProcessLockFile(kServiceProcessCloudPrint);
  DCHECK(ret) << "Failed to delete service process lock file.";
  return ret;
}

void ServiceProcess::Shutdown() {
  // Quit the main message loop.
  main_message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

CloudPrintProxy* ServiceProcess::GetCloudPrintProxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new CloudPrintProxy());
    cloud_print_proxy_->Initialize(service_prefs_.get());
  }
  return cloud_print_proxy_.get();
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
