// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
#pragma once

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/browser/worker_host/worker_service_observer.h"

namespace IPC {
class Message;
}

namespace content {

class DevToolsAgentHost;

// All methods are supposed to be called on the IO thread.
class WorkerDevToolsManager : private WorkerServiceObserver {
 public:
  // Returns the WorkerDevToolsManager singleton.
  static WorkerDevToolsManager* GetInstance();

  // Called on the UI thread.
  static DevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);

  void ForwardToDevToolsClient(int worker_process_id,
                               int worker_route_id,
                               const std::string& message);
  void SaveAgentRuntimeState(int worker_process_id,
                             int worker_route_id,
                             const std::string& state);

 private:
  friend struct DefaultSingletonTraits<WorkerDevToolsManager>;
  typedef std::pair<int, int> WorkerId;
  class AgentHosts;
  class DetachedClientHosts;
  class WorkerDevToolsAgentHost;
  struct InspectedWorker;
  typedef std::list<InspectedWorker> InspectedWorkersList;

  WorkerDevToolsManager();
  virtual ~WorkerDevToolsManager();

  // WorkerServiceOberver implementation.
  virtual void WorkerCreated(
      WorkerProcessHost* process,
      const WorkerProcessHost::WorkerInstance& instance) OVERRIDE;
  virtual void WorkerDestroyed(
      WorkerProcessHost* process,
      int worker_route_id) OVERRIDE;
  virtual void WorkerContextStarted(WorkerProcessHost* process,
                                    int worker_route_id) OVERRIDE;

  void RemoveInspectedWorkerData(const WorkerId& id);
  InspectedWorkersList::iterator FindInspectedWorker(int host_id, int route_id);

  void RegisterDevToolsAgentHostForWorker(int worker_process_id,
                                          int worker_route_id);
  void ForwardToWorkerDevToolsAgent(int worker_process_host_id,
                                    int worker_route_id,
                                    const IPC::Message& message);
  static void ForwardToDevToolsClientOnUIThread(
      int worker_process_id,
      int worker_route_id,
      const std::string& message);
  static void SaveAgentRuntimeStateOnUIThread(
      int worker_process_id,
      int worker_route_id,
      const std::string& state);
  static void NotifyWorkerDestroyedOnIOThread(int worker_process_id,
                                              int worker_route_id);
  static void NotifyWorkerDestroyedOnUIThread(int worker_process_id,
                                              int worker_route_id);
  static void SendResumeToWorker(const WorkerId& id);

  InspectedWorkersList inspected_workers_;

  struct TerminatedInspectedWorker;
  typedef std::list<TerminatedInspectedWorker> TerminatedInspectedWorkers;
  // List of terminated workers for which there may be a devtools client on
  // the UI thread. Worker entry is added into this list when inspected worker
  // is terminated and will be removed in one of two cases:
  // - shared worker with the same URL and name is started(in wich case we will
  // try to reattach existing DevTools client to the new worker).
  // - DevTools client which was inspecting terminated worker is closed on the
  // UI thread and and WorkerDevToolsManager is notified about that on the IO
  // thread.
  TerminatedInspectedWorkers terminated_workers_;

  typedef std::map<WorkerId, WorkerId> PausedWorkers;
  // Map from old to new worker id for the inspected workers that have been
  // terminated and started again in paused state. Worker data will be removed
  // from this list in one of two cases:
  // - DevTools client is closed on the UI thread, WorkerDevToolsManager was
  // notified about that on the IO thread and sent "resume" message to the
  // worker.
  // - Existing DevTools client was reattached to the new worker.
  PausedWorkers paused_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_WORKER_DEVTOOLS_MANAGER_H_
