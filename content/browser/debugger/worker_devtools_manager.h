// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}
class DevToolsAgentHost;
class WorkerDevToolsMessageFilter;

// All methods are supposed to be called on the IO thread.
class WorkerDevToolsManager {
 public:
  // Returns the WorkerDevToolsManager singleton.
  static WorkerDevToolsManager* GetInstance();

  // Called on the UI thread.
  static CONTENT_EXPORT DevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);

  void WorkerProcessDestroying(int worker_process_host_id);
  void ForwardToDevToolsClient(int worker_process_id,
                               int worker_route_id,
                               const IPC::Message& message);
 private:
  friend struct DefaultSingletonTraits<WorkerDevToolsManager>;
  class AgentHosts;
  class WorkerDevToolsAgentHost;

  WorkerDevToolsManager();
  ~WorkerDevToolsManager();

  void RegisterDevToolsAgentHostForWorker(int worker_process_id,
                                          int worker_route_id);
  void ForwardToWorkerDevToolsAgent(int worker_process_host_id,
                                    int worker_route_id,
                                    const IPC::Message& message);
  static void ForwardToDevToolsClientOnUIThread(
      int worker_process_id,
      int worker_route_id,
      const IPC::Message& message);
  static void NotifyWorkerDestroyedOnIOThread(int worker_process_id,
                                              int worker_route_id);
  static void NotifyWorkerDestroyedOnUIThread(int worker_process_id,
                                              int worker_route_id);

  class InspectedWorkersList;
  scoped_ptr<InspectedWorkersList> inspected_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsManager);
};

#endif  // CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
