// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "ipc/ipc_listener.h"

namespace content {

class BrowserContext;

class ServiceWorkerDevToolsAgentHost : public DevToolsAgentHostImpl,
                                       public IPC::Listener {
 public:
  using List = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using Map = std::map<std::string,
                       scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using ServiceWorkerIdentifier =
      ServiceWorkerDevToolsManager::ServiceWorkerIdentifier;

  ServiceWorkerDevToolsAgentHost(int worker_process_id,
                                 int worker_route_id,
                                 const ServiceWorkerIdentifier& service_worker,
                                 bool is_installed_version);

  // DevToolsAgentHost overrides.
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

  void PauseForDebugOnStart();
  bool IsPausedForDebugOnStart();
  bool IsReadyForInspection();
  void WorkerReadyForInspection();
  void WorkerRestarted(int worker_process_id, int worker_route_id);
  void WorkerDestroyed();
  void WorkerVersionInstalled();
  void WorkerVersionDoomed();

  GURL scope() const;

  // If the ServiceWorker has been installed before the worker instance started,
  // it returns the time when the instance started. Otherwise returns the time
  // when the ServiceWorker was installed.
  base::Time version_installed_time() const { return version_installed_time_; }

  // Returns the time when the ServiceWorker was doomed.
  base::Time version_doomed_time() const { return version_doomed_time_; }

  bool Matches(const ServiceWorkerIdentifier& other);

 private:
  enum WorkerState {
    WORKER_UNINSPECTED,
    WORKER_INSPECTED,
    WORKER_TERMINATED,
    WORKER_PAUSED_FOR_DEBUG_ON_START,
    WORKER_READY_FOR_DEBUG_ON_START,
    WORKER_PAUSED_FOR_REATTACH,
  };

  ~ServiceWorkerDevToolsAgentHost() override;

  void AttachToWorker();
  void DetachFromWorker();
  void OnDispatchOnInspectorFrontend(const DevToolsMessageChunk& message);

  WorkerState state_;
  int worker_process_id_;
  int worker_route_id_;
  std::unique_ptr<ServiceWorkerIdentifier> service_worker_;
  base::Time version_installed_time_;
  base::Time version_doomed_time_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
