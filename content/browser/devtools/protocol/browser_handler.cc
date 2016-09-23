// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/browser_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
namespace devtools {
namespace browser {

using Response = DevToolsProtocolClient::Response;

BrowserHandler::BrowserHandler()
    : weak_factory_(this) {
}

BrowserHandler::~BrowserHandler() {
}

void BrowserHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void BrowserHandler::Detached() {
  for (const auto& host : attached_hosts_)
    host->DetachClient(this);
  attached_hosts_.clear();
}

Response BrowserHandler::CreateBrowserContext(std::string* out_context_id) {
  // For layering reasons this needs to be handled by
  // DevToolsManagerDelegate::HandleCommand.
  return Response::ServerError("Not supported");
}

Response BrowserHandler::DisposeBrowserContext(const std::string& context_id,
                                               bool* out_success) {
  // For layering reasons this needs to be handled by
  // DevToolsManagerDelegate::HandleCommand.
  return Response::ServerError("Not supported");
}

Response BrowserHandler::CreateTarget(const std::string& url,
                                      const int* width,
                                      const int* height,
                                      const std::string* context_id,
                                      std::string* out_target_id) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError("Not supported");
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      delegate->CreateNewTarget(GURL(url));
  if (!agent_host)
    return Response::ServerError("Not supported");
  *out_target_id = agent_host->GetId();
  return Response::OK();
}

Response BrowserHandler::CloseTarget(const std::string& target_id,
                                     bool* out_success) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  *out_success = agent_host->Close();
  return Response::OK();
}

Response BrowserHandler::GetTargets(DevToolsCommandId command_id) {
  DevToolsAgentHost::DiscoverAllHosts(
      base::Bind(&BrowserHandler::RespondToGetTargets,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

void BrowserHandler::RespondToGetTargets(
    DevToolsCommandId command_id,
    DevToolsAgentHost::List agents) {
  std::vector<scoped_refptr<devtools::browser::TargetInfo>> infos;
  for (const auto& agent_host : agents) {
    scoped_refptr<devtools::browser::TargetInfo> info =
        devtools::browser::TargetInfo::Create()->
            set_target_id(agent_host->GetId())->
            set_type(agent_host->GetType())->
            set_title(agent_host->GetTitle())->
            set_url(agent_host->GetURL().spec());
    infos.push_back(info);
  }
  client_->SendGetTargetsResponse(
      command_id,
      GetTargetsResponse::Create()->set_target_info(std::move(infos)));
}

Response BrowserHandler::Attach(DevToolsCommandId command_id,
                                const std::string& target_id) {
  // Discover in order to get ahold of the items.
  DevToolsAgentHost::DiscoverAllHosts(
      base::Bind(&BrowserHandler::RespondToAttach,
                 weak_factory_.GetWeakPtr(), command_id, target_id));
  return Response::OK();
}

void BrowserHandler::RespondToAttach(DevToolsCommandId command_id,
                                     const std::string& target_id,
                                     DevToolsAgentHost::List agents) {
  // We were discovering to get ahold of the items, discard them.
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  bool success = false;
  if (agent_host)
    success = agent_host->AttachClient(this);
  attached_hosts_.push_back(agent_host);
  client_->SendAttachResponse(
      command_id,
      AttachResponse::Create()->set_success(success));
}

Response BrowserHandler::Detach(const std::string& target_id,
                                bool* out_success) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  auto it = std::find(
      attached_hosts_.begin(), attached_hosts_.end(), agent_host);
  if (it != attached_hosts_.end())
    attached_hosts_.erase(it);
  *out_success = agent_host && agent_host->DetachClient(this);
  return Response::OK();
}

Response BrowserHandler::SendMessage(const std::string& target_id,
                                     const std::string& message) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  agent_host->DispatchProtocolMessage(this, message);
  return Response::OK();
}

Response BrowserHandler::SetRemoteLocations(
    const std::vector<std::unique_ptr<base::DictionaryValue>>& locations) {
  return Response::OK();
}

void BrowserHandler::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {
  client_->DispatchMessage(DispatchMessageParams::Create()->
      set_target_id(agent_host->GetId())->
      set_message(message));
}

void BrowserHandler::AgentHostClosed(DevToolsAgentHost* agent_host,
                                     bool replaced_with_another_client) {
  auto it = std::find(
      attached_hosts_.begin(), attached_hosts_.end(), agent_host);
  if (it != attached_hosts_.end())
    attached_hosts_.erase(it);
}

}  // namespace browser
}  // namespace devtools
}  // namespace content
