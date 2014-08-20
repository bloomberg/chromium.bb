// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "ipc/ipc_listener.h"

namespace content {

class SharedWorkerInstance;

class EmbeddedWorkerDevToolsAgentHost : public IPCDevToolsAgentHost,
                                        public IPC::Listener {
 public:
  typedef EmbeddedWorkerDevToolsManager::WorkerId WorkerId;
  typedef EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier
      ServiceWorkerIdentifier;

  EmbeddedWorkerDevToolsAgentHost(WorkerId worker_id,
                                  const SharedWorkerInstance& shared_worker);

  EmbeddedWorkerDevToolsAgentHost(WorkerId worker_id,
                                  const ServiceWorkerIdentifier& service_worker,
                                  bool debug_service_worker_on_start);

  // DevToolsAgentHost override.
  virtual bool IsWorker() const OVERRIDE;

  // IPCDevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* message) OVERRIDE;
  virtual void Attach() OVERRIDE;
  virtual void OnClientAttached() OVERRIDE {}
  virtual void OnClientDetached() OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void WorkerReadyForInspection();
  void WorkerContextStarted();
  void WorkerRestarted(WorkerId worker_id);
  void WorkerDestroyed();
  bool Matches(const SharedWorkerInstance& other);
  bool Matches(const ServiceWorkerIdentifier& other);

  virtual GURL GetURL();
  virtual bool Close();

 private:
  friend class EmbeddedWorkerDevToolsManagerTest;

  virtual ~EmbeddedWorkerDevToolsAgentHost();

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
  void OnDispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRuntimeState(const std::string& state);

  scoped_ptr<SharedWorkerInstance> shared_worker_;
  scoped_ptr<ServiceWorkerIdentifier> service_worker_;
  WorkerState state_;
  WorkerId worker_id_;
  std::string saved_agent_state_;
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_AGENT_HOST_H_
