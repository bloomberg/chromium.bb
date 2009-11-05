// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_

#include <list>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"

class WorkerProcessHost;
class ResourceDispatcherHost;

class WorkerService : public NotificationObserver {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Initialize the WorkerService.  OK to be called multiple times.
  void Initialize(ResourceDispatcherHost* rdh);

  // Creates a dedicated worker.  Returns true on success.
  bool CreateDedicatedWorker(const GURL &url,
                             int renderer_pid,
                             int render_view_route_id,
                             IPC::Message::Sender* sender,
                             int sender_id,
                             int sender_route_id);

  // Cancel creation of a dedicated worker that hasn't started yet.
  void CancelCreateDedicatedWorker(int sender_id, int sender_route_id);

  // Called by the worker creator when a message arrives that should be
  // forwarded to the worker process.
  void ForwardMessage(const IPC::Message& message, int sender_id);

  int next_worker_route_id() { return ++next_worker_route_id_; }

  // TODO(dimich): This code assumes there is 1 worker per worker process, which
  // is how it is today until V8 can run in separate threads.
  const WorkerProcessHost::WorkerInstance* FindWorkerInstance(
      int worker_process_id);

  // Used when multiple workers can run in the same process.
  static const int kMaxWorkerProcessesWhenSharing;

  // Used when we run each worker in a separate process.
  static const int kMaxWorkersWhenSeparate;
  static const int kMaxWorkersPerTabWhenSeparate;

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  // Returns a WorkerProcessHost object if one exists for the given domain, or
  // NULL if there are no such workers yet.
  WorkerProcessHost* GetProcessForDomain(const GURL& url);

  // Returns a WorkerProcessHost based on a strategy of creating one worker per
  // core.
  WorkerProcessHost* GetProcessToFillUpCores();

  // Returns the WorkerProcessHost from the existing set that has the least
  // number of worker instance running.
  WorkerProcessHost* GetLeastLoadedWorker();

  // Checks if we can create a worker process based on the process limit when
  // we're using a strategy of one process per core.
  bool CanCreateWorkerProcess(
      const WorkerProcessHost::WorkerInstance& instance);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Notifies us that a process that's talking to a worker has shut down.
  void SenderShutdown(IPC::Message::Sender* sender);

  // Notifies us that a worker process has closed.
  void WorkerProcessDestroyed(WorkerProcessHost* process);

  NotificationRegistrar registrar_;
  int next_worker_route_id_;
  ResourceDispatcherHost* resource_dispatcher_host_;

  WorkerProcessHost::Instances queued_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
