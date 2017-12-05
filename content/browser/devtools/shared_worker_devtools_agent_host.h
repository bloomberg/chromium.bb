// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "base/macros.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "ipc/ipc_listener.h"

namespace content {

class SharedWorkerInstance;
class SharedWorkerHost;
class RenderProcessHost;

class SharedWorkerDevToolsAgentHost : public DevToolsAgentHostImpl,
                                      public IPC::Listener {
 public:
  using List = std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>;

  explicit SharedWorkerDevToolsAgentHost(SharedWorkerHost* worker_host);

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;
  std::string GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  void Reload() override;
  bool Close() override;

  // DevToolsAgentHostImpl overrides.
  void AttachSession(DevToolsSession* session) override;
  void DetachSession(int session_id) override;
  bool DispatchProtocolMessage(DevToolsSession* session,
                               const std::string& message) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  bool Matches(SharedWorkerHost* worker_host);
  void WorkerReadyForInspection();
  // Returns whether the worker should be paused for reattach.
  bool WorkerRestarted(SharedWorkerHost* worker_host);
  void WorkerDestroyed();

 private:
  friend class SharedWorkerDevToolsManagerTest;

  ~SharedWorkerDevToolsAgentHost() override;
  RenderProcessHost* GetProcess();
  void OnDispatchOnInspectorFrontend(const DevToolsMessageChunk& message);

  SharedWorkerHost* worker_host_;
  std::unique_ptr<SharedWorkerInstance> instance_;
  bool waiting_ready_for_reattach_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_H_
