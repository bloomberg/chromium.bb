// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <map>
#include <set>

#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

class RenderFrameHostImpl;

namespace devtools {
namespace target {

class TargetHandler : public DevToolsAgentHostClient,
                      public ServiceWorkerDevToolsManager::Observer {
 public:
  using Response = DevToolsProtocolClient::Response;

  TargetHandler();
  ~TargetHandler() override;

  void SetRenderFrameHost(RenderFrameHostImpl* render_frame_host);
  void SetClient(std::unique_ptr<Client> client);
  void Detached();

  void UpdateServiceWorkers();

  // Domain implementation.
  Response Enable();
  Response Disable();
  Response SetWaitForDebuggerOnStart(bool value);
  Response SendMessageToTarget(const std::string& target_id,
                               const std::string& message);
  Response GetTargetInfo(const std::string& target_id,
                         scoped_refptr<TargetInfo>* target_info);
  Response ActivateTarget(const std::string& target_id);

 private:
  void UpdateServiceWorkers(bool waiting_for_debugger);
  void AttachToTargetInternal(DevToolsAgentHost* host,
                              bool waiting_for_debugger);
  void DetachFromTargetInternal(DevToolsAgentHost* host);

  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerReadyForInspection(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

  // DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  std::unique_ptr<Client> client_;
  bool enabled_;
  bool wait_for_debugger_on_start_;
  RenderFrameHostImpl* render_frame_host_;
  std::map<std::string, scoped_refptr<DevToolsAgentHost>> attached_hosts_;
  std::set<GURL> frame_urls_;

  DISALLOW_COPY_AND_ASSIGN(TargetHandler);
};

}  // namespace target
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
