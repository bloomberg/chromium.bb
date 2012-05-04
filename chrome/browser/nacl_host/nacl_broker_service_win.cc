// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_broker_service_win.h"

#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"

using content::BrowserChildProcessHostIterator;

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

bool NaClBrokerService::LaunchLoader(
    base::WeakPtr<NaClProcessHost> nacl_process_host,
    const std::string& loader_channel_id) {
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

void NaClBrokerService::OnLoaderLaunched(const std::string& channel_id,
                                         base::ProcessHandle handle) {
  PendingLaunchesMap::iterator it = pending_launches_.find(channel_id);
  if (pending_launches_.end() == it)
    NOTREACHED();

  NaClProcessHost* client = it->second;
  if (client)
    client->OnProcessLaunchedByBroker(handle);
  pending_launches_.erase(it);
  ++loaders_running_;
}

void NaClBrokerService::OnLoaderDied() {
  DCHECK(loaders_running_ > 0);
  --loaders_running_;
  // Stop the broker only if there are no loaders running or being launched.
  NaClBrokerHost* broker_host = GetBrokerHost();
  if (loaders_running_ + pending_launches_.size() == 0 && broker_host != NULL) {
    broker_host->StopBroker();
  }
}

bool NaClBrokerService::LaunchDebugExceptionHandler(
    base::WeakPtr<NaClProcessHost> nacl_process_host, int32 pid,
    base::ProcessHandle process_handle, const std::string& startup_info) {
  pending_debuggers_[pid] = nacl_process_host;
  NaClBrokerHost* broker_host = GetBrokerHost();
  if (!broker_host)
    return false;
  return broker_host->LaunchDebugExceptionHandler(pid, process_handle,
                                                  startup_info);
}

void NaClBrokerService::OnDebugExceptionHandlerLaunched(int32 pid,
                                                        bool success) {
  PendingDebugExceptionHandlersMap::iterator it = pending_debuggers_.find(pid);
  if (pending_debuggers_.end() == it)
    NOTREACHED();

  NaClProcessHost* client = it->second;
  if (client)
    client->OnDebugExceptionHandlerLaunchedByBroker(success);
  pending_debuggers_.erase(it);
}

NaClBrokerHost* NaClBrokerService::GetBrokerHost() {
  BrowserChildProcessHostIterator iter(content::PROCESS_TYPE_NACL_BROKER);
  if (iter.Done())
    return NULL;
  return static_cast<NaClBrokerHost*>(iter.GetDelegate());
}
