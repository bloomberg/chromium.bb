// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_broker_service_win.h"

#include "chrome/browser/nacl_host/nacl_process_host.h"

NaClBrokerService* NaClBrokerService::GetInstance() {
  return Singleton<NaClBrokerService>::get();
}

NaClBrokerService::NaClBrokerService()
    : loaders_running_(0) {
}

bool NaClBrokerService::StartBroker() {
  NaClBrokerHost* broker_host = new NaClBrokerHost;
  if (!broker_host->Init()) {
    delete broker_host;
    return false;
  }
  return true;
}

bool NaClBrokerService::LaunchLoader(NaClProcessHost* nacl_process_host,
                                     const std::wstring& loader_channel_id) {
  // Add task to the list
  pending_launches_[loader_channel_id] = nacl_process_host;
  NaClBrokerHost* broker_host = GetBrokerHost();

  if (!broker_host) {
    if (!StartBroker())
      return false;
    broker_host = GetBrokerHost();
  }
  broker_host->LaunchLoader(loader_channel_id);

  return true;
}

void NaClBrokerService::OnLoaderLaunched(const std::wstring& channel_id,
                                         base::ProcessHandle handle) {
  NaClProcessHost* client;
  PendingLaunchesMap::iterator it = pending_launches_.find(channel_id);
  if (pending_launches_.end() == it)
    NOTREACHED();

  client = it->second;
  client->OnProcessLaunchedByBroker(handle);
  pending_launches_.erase(it);
  ++loaders_running_;
}

void NaClBrokerService::OnLoaderDied() {
  --loaders_running_;
  // Stop the broker only if there are no loaders running or being launched.
  NaClBrokerHost* broker_host = GetBrokerHost();
  if (loaders_running_ + pending_launches_.size() == 0 && broker_host != NULL) {
    broker_host->StopBroker();
  }
}

NaClBrokerHost* NaClBrokerService::GetBrokerHost() {
  for (BrowserChildProcessHost::Iterator iter(
           ChildProcessInfo::NACL_BROKER_PROCESS);
       !iter.Done();
       ++iter) {
    NaClBrokerHost* broker_host = static_cast<NaClBrokerHost*>(*iter);
    return broker_host;
  }
  return NULL;
}
