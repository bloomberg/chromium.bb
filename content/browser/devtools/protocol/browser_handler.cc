// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/browser_handler.h"

#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
namespace devtools {
namespace browser {

using Response = DevToolsProtocolClient::Response;

BrowserHandler::BrowserHandler() {
}

BrowserHandler::~BrowserHandler() {
}

void BrowserHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
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

Response BrowserHandler::GetTargets(TargetInfos* infos) {
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    DevToolsAgentHost* agent_host = (*it).get();
    scoped_refptr<devtools::browser::TargetInfo> info =
        devtools::browser::TargetInfo::Create()->
            set_target_id(agent_host->GetId())->
            set_type(agent_host->GetType())->
            set_title(agent_host->GetTitle())->
            set_url(agent_host->GetURL().spec());
    infos->push_back(info);
  }
  return Response::OK();
}

Response BrowserHandler::Attach(const std::string& targetId) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  bool success = agent_host->AttachClient(this);
  return success ? Response::OK() :
                   Response::ServerError("Target is already being debugged");
}

Response BrowserHandler::Detach(const std::string& targetId) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  bool success = agent_host->DetachClient(this);
  return success ? Response::OK() :
                   Response::ServerError("Target is not being debugged");
}

Response BrowserHandler::SendMessage(const std::string& targetId,
                                     const std::string& message) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(targetId);
  if (!agent_host)
    return Response::ServerError("No target with given id found");
  agent_host->DispatchProtocolMessage(this, message);
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
}

}  // namespace browser
}  // namespace devtools
}  // namespace content
