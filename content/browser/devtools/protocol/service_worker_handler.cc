// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "content/public/browser/devtools_agent_host.h"

namespace content {
namespace devtools {
namespace service_worker {

using Response = DevToolsProtocolClient::Response;

ServiceWorkerHandler::ServiceWorkerHandler() {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
  Disable();
}

void ServiceWorkerHandler::SetClient(
    scoped_ptr<Client> client) {
  client_.swap(client);
}

void ServiceWorkerHandler::Detached() {
  Disable();
}

Response ServiceWorkerHandler::Enable() {
  DevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&agent_hosts);
  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);
  for (auto host : agent_hosts)
    WorkerCreated(host.get());
  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  for (const auto& pair : attached_hosts_)
    pair.second->DetachClient();
  attached_hosts_.clear();
  return Response::OK();
}

Response ServiceWorkerHandler::SendMessage(
    const std::string& worker_id,
    const std::string& message) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");

  it->second->DispatchProtocolMessage(message);
  return Response::OK();
}

Response ServiceWorkerHandler::Attach(const std::string& worker_id) {
  scoped_refptr<DevToolsAgentHost> host =
      DevToolsAgentHost::GetForId(worker_id);
  if (!host)
    return Response::InternalError("No such worker available");

  if (host->IsAttached())
    return Response::InternalError("Another client is already attached");

  attached_hosts_[worker_id] = host;
  host->AttachClient(this);
  return Response::OK();
}

Response ServiceWorkerHandler::Detach(const std::string& worker_id) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");

  attached_hosts_.erase(worker_id);
  it->second->DetachClient();
  return Response::OK();
}

void ServiceWorkerHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  client_->DispatchMessage(
      DispatchMessageParams::Create()->
          set_worker_id(host->GetId())->
          set_message(message));
}

void ServiceWorkerHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  WorkerDestroyed(host);
}

void ServiceWorkerHandler::WorkerCreated(
    DevToolsAgentHost* host) {
  client_->WorkerCreated(WorkerCreatedParams::Create()->
      set_worker_id(host->GetId())->
      set_url(host->GetURL().spec()));
}

void ServiceWorkerHandler::WorkerDestroyed(
    DevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(it);
}

}  // namespace service_worker
}  // namespace devtools
}  // namespace content
