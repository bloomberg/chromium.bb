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
    : observer_(observer) {
}

void ChromotingHostManager::Initialize(
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

  chromoting_config_ = config;

  if (!IsConfigInitialized()) {
    InitializeConfig();
  }

  if (IsEnabled()) {
    Start();
  }
}

void ChromotingHostManager::Teardown() {
  Stop();
}

ChromotingHostManager::~ChromotingHostManager() {}

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
  if (IsEnabled()) {
    SetEnabled(false);
    observer_->OnChromotingHostDisabled();
  }

  Stop();
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

void ChromotingHostManager::Stop() {
  // Stop the host if it is started.
  if (chromoting_host_) {
    // Shutdown the chromoting host asynchronously. This will signal the host to
    // shutdown, we'll actually wait for all threads to stop when we destroy
    // the chromoting context.
    chromoting_host_->Shutdown();
    chromoting_host_ = NULL;

    chromoting_context_->Stop();
    chromoting_context_.reset();
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
  chromoting_context_.reset(new remoting::ChromotingHostContext());
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
}

}  // namespace remoting
