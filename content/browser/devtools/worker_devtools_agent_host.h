// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "base/macros.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"

namespace content {

class BrowserContext;

class CONTENT_EXPORT WorkerDevToolsAgentHost : public DevToolsAgentHostImpl,
                                               public IPC::Listener {
 public:
  typedef std::pair<int, int> WorkerId;

  // DevToolsAgentHost override.
  BrowserContext* GetBrowserContext() override;

  // DevToolsAgentHostImpl overrides.
  void AttachSession(DevToolsSession* session) override;
  void DetachSession(int session_id) override;
  bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  void PauseForDebugOnStart();
  bool IsPausedForDebugOnStart();
  bool IsReadyForInspection();

  void WorkerReadyForInspection();
  void WorkerRestarted(WorkerId worker_id);
  void WorkerDestroyed();
  bool IsTerminated();

 protected:
  explicit WorkerDevToolsAgentHost(WorkerId worker_id);
  ~WorkerDevToolsAgentHost() override;

  enum WorkerState {
    WORKER_UNINSPECTED,
    WORKER_INSPECTED,
    WORKER_TERMINATED,
    WORKER_PAUSED_FOR_DEBUG_ON_START,
    WORKER_READY_FOR_DEBUG_ON_START,
    WORKER_PAUSED_FOR_REATTACH,
  };

  virtual void OnAttachedStateChanged(bool attached);
  const WorkerId& worker_id() const { return worker_id_; }

 private:
  friend class SharedWorkerDevToolsManagerTest;

  void AttachToWorker();
  void DetachFromWorker();
  void WorkerCreated();
  void OnDispatchOnInspectorFrontend(const DevToolsMessageChunk& message);

  WorkerState state_;
  WorkerId worker_id_;
  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_AGENT_HOST_H_
