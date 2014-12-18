// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "ipc/ipc_listener.h"

namespace content {

class BrowserContext;
class SharedWorkerInstance;

class EmbeddedWorkerDevToolsAgentHost : public IPCDevToolsAgentHost,
                                        public IPC::Listener {
 public:
  typedef EmbeddedWorkerDevToolsManager::WorkerId WorkerId;
  typedef EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier
      ServiceWorkerIdentifier;

  // DevToolsAgentHost override.
  bool IsWorker() const override;
  BrowserContext* GetBrowserContext() override;

  // IPCDevToolsAgentHost implementation.
  void SendMessageToAgent(IPC::Message* message) override;
  void Attach() override;
  void OnClientAttached() override;
  void OnClientDetached() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  void WorkerReadyForInspection();
  void WorkerRestarted(WorkerId worker_id);
  void WorkerDestroyed();
  bool IsTerminated();

  // TODO(kinuko): Remove these virtual methods after we split devtools manager.
  virtual bool Matches(const SharedWorkerInstance& other);
  virtual bool Matches(const ServiceWorkerIdentifier& other);

 protected:
  friend class EmbeddedWorkerDevToolsManagerTest;

  EmbeddedWorkerDevToolsAgentHost(WorkerId worker_id);
  ~EmbeddedWorkerDevToolsAgentHost() override;

  enum WorkerState {
    WORKER_UNINSPECTED,
    WORKER_INSPECTED,
    WORKER_TERMINATED,
    WORKER_PAUSED_FOR_DEBUG_ON_START,
    WORKER_PAUSED_FOR_REATTACH,
  };

  void AttachToWorker();
  void DetachFromWorker();
  void WorkerCreated();
  void OnDispatchOnInspectorFrontend(const std::string& message,
                                     uint32 total_size);
  void OnSaveAgentRuntimeState(const std::string& state);

  void set_state(WorkerState state) { state_ = state; }
  const WorkerId& worker_id() const { return worker_id_; }

 private:
  WorkerState state_;
  WorkerId worker_id_;
  std::string saved_agent_state_;
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_
