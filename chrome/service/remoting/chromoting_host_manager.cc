// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/remoting/chromoting_host_manager.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "chrome/common/remoting/chromoting_host_info.h"
#include "net/base/net_util.h"
#include "remoting/base/constants.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/json_host_config.h"

namespace remoting {

ChromotingHostManager::ChromotingHostManager(Observer* observer)
    : observer_(observer),
      main_message_loop_(NULL) {
}

void ChromotingHostManager::Initialize(
    MessageLoopForUI* main_message_loop,
    base::MessageLoopProxy* file_message_loop) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath chromoting_config_path =
      user_data_dir.Append(FILE_PATH_LITERAL(".ChromotingConfig.json"));
  remoting::JsonHostConfig* config = new remoting::JsonHostConfig(
      chromoting_config_path, file_message_loop);
  if (!config->Read()) {
    VLOG(1) << "Failed to read chromoting config file.";
  }

  main_message_loop_ = main_message_loop;
  chromoting_config_ = config;

  if (!IsConfigInitialized()) {
    InitializeConfig();
  }

  if (IsEnabled()) {
    // TODO(wez): Need to callback the Observer so that ServiceProcess
    // knows to stay alive to service Chromoting requests.
    // This will go away once we have a more consistent model for the
    // service process internals.
    observer_->OnChromotingHostEnabled();
    Start();
  }
}

void ChromotingHostManager::Teardown(Task* done_task) {
  Stop(done_task);
}

ChromotingHostManager::~ChromotingHostManager() {
  DCHECK(!chromoting_host_);
  DCHECK(!chromoting_context_.get());
}

bool ChromotingHostManager::IsConfigInitialized() {
  std::string host_id;
  if (!chromoting_config_->GetString(remoting::kHostIdConfigPath, &host_id))
    return false;

  return guid::IsValidGUID(host_id);
}

void ChromotingHostManager::InitializeConfig() {
  VLOG(1) << "Initializing static chromoting host parameters.";

  // TODO(hclam): This is a time consuming operation so we should run it on
  // a separate thread.
  remoting::HostKeyPair host_key_pair;
  host_key_pair.Generate();
  std::string host_id(guid::GenerateGUID());
  std::string hostname(net::GetHostName());

  chromoting_config_->SetBoolean(remoting::kHostEnabledConfigPath, false);
  chromoting_config_->SetString(remoting::kHostIdConfigPath, host_id);
  chromoting_config_->SetString(remoting::kHostNameConfigPath, hostname);
  host_key_pair.Save(chromoting_config_);

  // Save updated values on the disk.
  chromoting_config_->Save();
}

void ChromotingHostManager::SetCredentials(const std::string& login,
                                           const std::string& token) {
  chromoting_config_->SetString(remoting::kXmppLoginConfigPath, login);
  chromoting_config_->SetString(remoting::kXmppAuthTokenConfigPath, token);

  // Save updated values on the disk.
  chromoting_config_->Save();
}

void ChromotingHostManager::Enable() {
  // We have already started.
  if (IsEnabled())
    return;

  SetEnabled(true);
  observer_->OnChromotingHostEnabled();

  Start();
}

void ChromotingHostManager::Disable() {
  if (!IsEnabled())
    return;

  SetEnabled(false);

  // TODO(hclam): Immediately reporting will cause threading problems
  // ServiceProcess thinks we can shutdown safely.
  observer_->OnChromotingHostDisabled();

  Stop(NULL);
}

void ChromotingHostManager::GetHostInfo(ChromotingHostInfo* host_info) {
  chromoting_config_->GetString(remoting::kHostIdConfigPath,
                                &host_info->host_id);
  chromoting_config_->GetString(remoting::kHostNameConfigPath,
                                &host_info->hostname);
  HostKeyPair key_pair;
  if (key_pair.Load(chromoting_config_)) {
    host_info->public_key = key_pair.GetPublicKey();
  }

  host_info->enabled = IsEnabled();

  chromoting_config_->GetString(remoting::kXmppLoginConfigPath,
                                &host_info->login);
}

void ChromotingHostManager::Stop(Task* done_task) {
  // Stop the host if it is started.
  if (chromoting_host_) {
    // Save the shutdown task, it will be executed when chromoting host
    // is stopped.
    shutdown_task_.reset(done_task);

    // Shutdown the chromoting host asynchronously.
    chromoting_host_->Shutdown();
  } else if (done_task) {
    done_task->Run();
    delete done_task;
  }
}

bool ChromotingHostManager::IsEnabled() {
  bool enabled;
  if (!chromoting_config_->GetBoolean(remoting::kHostEnabledConfigPath,
                                      &enabled)) {
    enabled = false;
  }
  return enabled;
}

void ChromotingHostManager::SetEnabled(bool enabled) {
  chromoting_config_->SetBoolean(remoting::kHostEnabledConfigPath,
                                 enabled);
  chromoting_config_->Save();
}

void ChromotingHostManager::Start() {
  // Don't do anything if we already started.
  if (chromoting_host_.get())
    return;

  // Start the chromoting context first.
  chromoting_context_.reset(
      new remoting::ChromotingHostContext(main_message_loop_));
  chromoting_context_->Start();

  // Create a chromoting host object.
  chromoting_host_ = remoting::ChromotingHost::Create(chromoting_context_.get(),
                                                      chromoting_config_);

  // Then start the chromoting host.
  // When ChromotingHost is shutdown because of failure or a request that
  // we made OnChromotingShutdown() is calls.
  chromoting_host_->Start(
      NewRunnableMethod(this, &ChromotingHostManager::OnShutdown));
}

void ChromotingHostManager::OnShutdown() {
  if (MessageLoop::current() != main_message_loop_) {
    main_message_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChromotingHostManager::OnShutdown));
    return;
  }
  chromoting_context_->Stop();
  chromoting_context_.reset();
  chromoting_host_ = NULL;

  if (shutdown_task_.get()) {
    shutdown_task_->Run();
    shutdown_task_.reset();
  }
}

}  // namespace remoting
