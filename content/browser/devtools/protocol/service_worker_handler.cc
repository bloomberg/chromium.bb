// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"

namespace content {
namespace devtools {
namespace service_worker {

using Response = DevToolsProtocolClient::Response;

ServiceWorkerHandler::ServiceWorkerHandler()
    : enabled_(false) {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
  Disable();
}

void ServiceWorkerHandler::SetClient(
    scoped_ptr<Client> client) {
  client_.swap(client);
}

void ServiceWorkerHandler::SetURL(const GURL& url) {
  url_ = url;
  if (enabled_) {
    for (auto pair : attached_hosts_) {
      if (!MatchesInspectedPage(pair.second.get()))
        WorkerDestroyed(pair.second.get());
    }

    ServiceWorkerDevToolsAgentHost::List agent_hosts;
    ServiceWorkerDevToolsManager::GetInstance()->
        AddAllAgentHosts(&agent_hosts);
    for (auto host : agent_hosts) {
      if (!MatchesInspectedPage(host.get()))
        continue;
      if (attached_hosts_.find(host->GetId()) != attached_hosts_.end())
        continue;
      // TODO(pfeldman): workers are created concurrently, we need
      // to get notification earlier to go through the Created/Ready
      // lifecycle.
      WorkerReadyForInspection(host.get());
    }
  }
}

void ServiceWorkerHandler::Detached() {
  Disable();
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::OK();
  enabled_ = true;

  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);

  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&agent_hosts);
  for (auto host : agent_hosts)
    WorkerReadyForInspection(host.get());
  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;

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

Response ServiceWorkerHandler::Stop(
    const std::string& worker_id) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->Close();
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
  WorkerDestroyed(static_cast<ServiceWorkerDevToolsAgentHost*>(host));
}

void ServiceWorkerHandler::WorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  if (!MatchesInspectedPage(host))
    return;
  host->PauseForDebugOnStart();
}

void ServiceWorkerHandler::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  if (host->IsAttached() || !MatchesInspectedPage(host))
    return;

  attached_hosts_[host->GetId()] = host;
  host->AttachClient(this);
  client_->WorkerCreated(WorkerCreatedParams::Create()->
      set_worker_id(host->GetId())->
      set_url(host->GetURL().spec()));
}

void ServiceWorkerHandler::WorkerDestroyed(
    ServiceWorkerDevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  it->second->DetachClient();
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(it);
}

bool ServiceWorkerHandler::MatchesInspectedPage(
    ServiceWorkerDevToolsAgentHost* host) {
  // TODO(pfeldman): match based on scope.
  // TODO(pfeldman): match iframes.
  return host->GetURL().host() == url_.host();
}

}  // namespace service_worker
}  // namespace devtools
}  // namespace content
