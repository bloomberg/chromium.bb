// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_handler.h"

#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"

namespace content {
namespace protocol {

namespace {

std::unique_ptr<Target::TargetInfo> CreateInfo(DevToolsAgentHost* host) {
  return Target::TargetInfo::Create()
      .SetTargetId(host->GetId())
      .SetTitle(host->GetTitle())
      .SetUrl(host->GetURL().spec())
      .SetType(host->GetType())
      .SetAttached(host->IsAttached())
      .Build();
}

}  // namespace

TargetHandler::TargetHandler()
    : DevToolsDomainHandler(Target::Metainfo::domainName),
      auto_attacher_(base::Bind(&TargetHandler::AttachToTargetInternal,
                                base::Unretained(this)),
                     base::Bind(&TargetHandler::DetachFromTargetInternal,
                                base::Unretained(this))),
      discover_(false) {}

TargetHandler::~TargetHandler() {
}

// static
std::vector<TargetHandler*> TargetHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return DevToolsSession::HandlersForAgentHost<TargetHandler>(
      host, Target::Metainfo::domainName);
}

void TargetHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Target::Frontend(dispatcher->channel()));
  Target::Dispatcher::wire(dispatcher, this);
}

void TargetHandler::SetRenderFrameHost(RenderFrameHostImpl* render_frame_host) {
  auto_attacher_.SetRenderFrameHost(render_frame_host);
}

Response TargetHandler::Disable() {
  SetAutoAttach(false, false);
  SetDiscoverTargets(false);
  for (const auto& id_host : attached_hosts_)
    id_host.second->DetachClient(this);
  attached_hosts_.clear();
  return Response::OK();
}

void TargetHandler::DidCommitNavigation() {
  auto_attacher_.UpdateServiceWorkers();
}

void TargetHandler::RenderFrameHostChanged() {
  auto_attacher_.UpdateFrames();
}

void TargetHandler::TargetCreatedInternal(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host->GetId()) != reported_hosts_.end())
    return;
  frontend_->TargetCreated(CreateInfo(host));
  reported_hosts_[host->GetId()] = host;
}

void TargetHandler::TargetInfoChangedInternal(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host->GetId()) == reported_hosts_.end())
    return;
  frontend_->TargetInfoChanged(CreateInfo(host));
}

void TargetHandler::TargetDestroyedInternal(DevToolsAgentHost* host) {
  auto it = reported_hosts_.find(host->GetId());
  if (it == reported_hosts_.end())
    return;
  if (discover_)
    frontend_->TargetDestroyed(host->GetId());
  reported_hosts_.erase(it);
}

bool TargetHandler::AttachToTargetInternal(
    DevToolsAgentHost* host, bool waiting_for_debugger) {
  attached_hosts_[host->GetId()] = host;
  if (!host->AttachClient(this)) {
    attached_hosts_.erase(host->GetId());
    return false;
  }
  frontend_->AttachedToTarget(CreateInfo(host), waiting_for_debugger);
  return true;
}

void TargetHandler::DetachFromTargetInternal(DevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  host->DetachClient(this);
  frontend_->DetachedFromTarget(host->GetId());
  attached_hosts_.erase(it);
}

// ----------------- Protocol ----------------------

Response TargetHandler::SetDiscoverTargets(bool discover) {
  if (discover_ == discover)
    return Response::OK();
  discover_ = discover;
  if (discover_) {
    DevToolsAgentHost::AddObserver(this);
  } else {
    DevToolsAgentHost::RemoveObserver(this);
    RawHostsMap copy = reported_hosts_;
    for (const auto& id_host : copy)
      TargetDestroyedInternal(id_host.second);
  }
  return Response::OK();
}

Response TargetHandler::SetAutoAttach(
    bool auto_attach, bool wait_for_debugger_on_start) {
  auto_attacher_.SetAutoAttach(auto_attach, wait_for_debugger_on_start);
  return Response::FallThrough();
}

Response TargetHandler::SetAttachToFrames(bool value) {
  auto_attacher_.SetAttachToFrames(value);
  return Response::OK();
}

Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<Target::RemoteLocation>>) {
  return Response::Error("Not supported");
}

Response TargetHandler::AttachToTarget(const std::string& target_id,
                                       bool* out_success) {
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *out_success = AttachToTargetInternal(agent_host.get(), false);
  return Response::OK();
}

Response TargetHandler::DetachFromTarget(const std::string& target_id) {
  auto it = attached_hosts_.find(target_id);
  if (it == attached_hosts_.end())
    return Response::Error("Not attached to the target");
  DevToolsAgentHost* agent_host = it->second.get();
  DetachFromTargetInternal(agent_host);
  return Response::OK();
}

Response TargetHandler::SendMessageToTarget(
    const std::string& target_id,
    const std::string& message) {
  auto it = attached_hosts_.find(target_id);
  if (it == attached_hosts_.end())
    return Response::FallThrough();
  it->second->DispatchProtocolMessage(this, message);
  return Response::OK();
}

Response TargetHandler::GetTargetInfo(
    const std::string& target_id,
    std::unique_ptr<Target::TargetInfo>* target_info) {
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *target_info = CreateInfo(agent_host.get());
  return Response::OK();
}

Response TargetHandler::ActivateTarget(const std::string& target_id) {
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  agent_host->Activate();
  return Response::OK();
}

Response TargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *out_success = agent_host->Close();
  return Response::OK();
}

Response TargetHandler::CreateBrowserContext(std::string* out_context_id) {
  return Response::Error("Not supported");
}

Response TargetHandler::DisposeBrowserContext(const std::string& context_id,
                                              bool* out_success) {
  return Response::Error("Not supported");
}

Response TargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     std::string* out_target_id) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Error("Not supported");
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      delegate->CreateNewTarget(GURL(url));
  if (!agent_host)
    return Response::Error("Not supported");
  *out_target_id = agent_host->GetId();
  return Response::OK();
}

Response TargetHandler::GetTargets(
    std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos) {
  *target_infos = protocol::Array<Target::TargetInfo>::create();
  for (const auto& host : DevToolsAgentHost::GetOrCreateAll())
    (*target_infos)->addItem(CreateInfo(host.get()));
  return Response::OK();
}

// ---------------- DevToolsAgentHostClient ----------------

void TargetHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  frontend_->ReceivedMessageFromTarget(host->GetId(), message);
}

void TargetHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  frontend_->DetachedFromTarget(host->GetId());
  attached_hosts_.erase(host->GetId());
  auto_attacher_.AgentHostClosed(host);
}

// -------------- DevToolsAgentHostObserver -----------------

bool TargetHandler::ShouldForceDevToolsAgentHostCreation() {
  return true;
}

void TargetHandler::DevToolsAgentHostCreated(DevToolsAgentHost* agent_host) {
  // If we start discovering late, all existing agent hosts will be reported,
  // but we could have already attached to some.
  TargetCreatedInternal(agent_host);
}

void TargetHandler::DevToolsAgentHostDestroyed(DevToolsAgentHost* agent_host) {
  DCHECK(attached_hosts_.find(agent_host->GetId()) == attached_hosts_.end());
  TargetDestroyedInternal(agent_host);
}

void TargetHandler::DevToolsAgentHostAttached(DevToolsAgentHost* host) {
  TargetInfoChangedInternal(host);
}

void TargetHandler::DevToolsAgentHostDetached(DevToolsAgentHost* host) {
  TargetInfoChangedInternal(host);
}

}  // namespace protocol
}  // namespace content
