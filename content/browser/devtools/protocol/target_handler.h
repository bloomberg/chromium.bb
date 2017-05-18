// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <map>
#include <set>

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/target.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_observer.h"

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class TargetHandler : public DevToolsDomainHandler,
                      public Target::Backend,
                      public DevToolsAgentHostClient,
                      public ServiceWorkerDevToolsManager::Observer,
                      public DevToolsAgentHostObserver {
 public:
  TargetHandler();
  ~TargetHandler() override;

  static std::vector<TargetHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;
  Response Disable() override;

  void UpdateServiceWorkers();
  void UpdateFrames();

  // Domain implementation.
  Response SetDiscoverTargets(bool discover) override;
  Response SetAutoAttach(bool auto_attach,
                         bool wait_for_debugger_on_start) override;
  Response SetAttachToFrames(bool value) override;
  Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<Target::RemoteLocation>>) override;
  Response AttachToTarget(const std::string& target_id,
                          bool* out_success) override;
  Response DetachFromTarget(const std::string& target_id) override;
  Response SendMessageToTarget(const std::string& target_id,
                               const std::string& message) override;
  Response GetTargetInfo(
      const std::string& target_id,
      std::unique_ptr<Target::TargetInfo>* target_info) override;
  Response ActivateTarget(const std::string& target_id) override;
  Response CloseTarget(const std::string& target_id,
                       bool* out_success) override;
  Response CreateBrowserContext(std::string* out_context_id) override;
  Response DisposeBrowserContext(const std::string& context_id,
                                 bool* out_success) override;
  Response CreateTarget(const std::string& url,
                        Maybe<int> width,
                        Maybe<int> height,
                        Maybe<std::string> context_id,
                        std::string* out_target_id) override;
  Response GetTargets(
      std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos)
      override;

 private:
  using HostsMap = std::map<std::string, scoped_refptr<DevToolsAgentHost>>;
  using RawHostsMap = std::map<std::string, DevToolsAgentHost*>;

  void UpdateServiceWorkers(bool waiting_for_debugger);
  void ReattachTargetsOfType(const HostsMap& new_hosts,
                             const std::string& type,
                             bool waiting_for_debugger);
  void TargetCreatedInternal(DevToolsAgentHost* host);
  void TargetDestroyedInternal(DevToolsAgentHost* host);
  bool AttachToTargetInternal(DevToolsAgentHost* host,
                              bool waiting_for_debugger);
  void DetachFromTargetInternal(DevToolsAgentHost* host);

  // ServiceWorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerReadyForInspection(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

  // DevToolsAgentHostObserver implementation.
  bool ShouldForceDevToolsAgentHostCreation() override;
  void DevToolsAgentHostCreated(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDestroyed(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostAttached(DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(DevToolsAgentHost* agent_host) override;

  // DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  std::unique_ptr<Target::Frontend> frontend_;
  bool discover_;
  bool auto_attach_;
  bool wait_for_debugger_on_start_;
  bool attach_to_frames_;
  RenderFrameHostImpl* render_frame_host_;
  HostsMap attached_hosts_;
  std::set<GURL> frame_urls_;
  RawHostsMap reported_hosts_;

  DISALLOW_COPY_AND_ASSIGN(TargetHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
