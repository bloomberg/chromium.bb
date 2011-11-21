// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/browser/worker_host/worker_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TaskManagerSharedWorkerResource;

class TaskManagerWorkerResourceProvider
    : public TaskManager::ResourceProvider,
      private WorkerServiceObserver,
      private content::NotificationObserver {
 public:
  explicit TaskManagerWorkerResourceProvider(TaskManager* task_manager);

 private:
  class WorkerResourceListHolder;
  typedef std::vector<TaskManagerSharedWorkerResource*> WorkerResourceList;
  typedef scoped_ptr<TaskManagerSharedWorkerResource> WorkerResourceHolder;
  typedef std::map<int, WorkerResourceList> ProcessIdToWorkerResources;

  virtual ~TaskManagerWorkerResourceProvider();

  // TaskManager::ResourceProvider implementation.
  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // WorkerServiceObserver implementation.
  virtual void WorkerCreated(
      WorkerProcessHost* process,
      const WorkerProcessHost::WorkerInstance& instance) OVERRIDE;
  virtual void WorkerDestroyed(
      WorkerProcessHost* process,
      int worker_route_id) OVERRIDE;
  virtual void WorkerContextStarted(WorkerProcessHost*, int) OVERRIDE {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void NotifyWorkerCreated(WorkerResourceHolder* resource_holder);
  void NotifyWorkerDestroyed(int process_id, int routing_id);

  void StartObservingWorkers();
  void StopObservingWorkers();

  void AddWorkerResourceList(WorkerResourceListHolder* resource_list_holder);
  void AddResource(TaskManagerSharedWorkerResource* resource);
  void DeleteAllResources();

  bool updating_;
  TaskManager* task_manager_;
  WorkerResourceList resources_;
  // Map from worker process id to the list of its workers. This list contains
  // entries for worker processes which has not launched yet but for which we
  // have already received WorkerCreated event. We don't add such workers to
  // the task manager until the process is launched.
  ProcessIdToWorkerResources launching_workers_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerWorkerResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_
