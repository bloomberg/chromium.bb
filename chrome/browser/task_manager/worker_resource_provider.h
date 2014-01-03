// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/worker_service_observer.h"

class TaskManager;

namespace task_manager {

class SharedWorkerResource;

class WorkerResourceProvider : public ResourceProvider,
                               public content::BrowserChildProcessObserver,
                               private content::WorkerServiceObserver {
 public:
  explicit WorkerResourceProvider(TaskManager* task_manager);

 private:
  class WorkerResourceListHolder;
  typedef std::vector<SharedWorkerResource*> WorkerResourceList;
  typedef scoped_ptr<SharedWorkerResource> WorkerResourceHolder;
  typedef std::map<int, WorkerResourceList> ProcessIdToWorkerResources;

  virtual ~WorkerResourceProvider();

  // ResourceProvider implementation.
  virtual Resource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::BrowserChildProcessObserver implementation.
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;

  // content::WorkerServiceObserver implementation.
  virtual void WorkerCreated(const GURL& url,
                             const base::string16& name,
                             int process_id,
                             int route_id) OVERRIDE;
  virtual void WorkerDestroyed(int process_id, int route_id) OVERRIDE;

  void NotifyWorkerCreated(WorkerResourceHolder* resource_holder);
  void NotifyWorkerDestroyed(int process_id, int routing_id);

  void StartObservingWorkers();
  void StopObservingWorkers();

  void AddWorkerResourceList(WorkerResourceListHolder* resource_list_holder);
  void AddResource(SharedWorkerResource* resource);
  void DeleteAllResources();

  bool updating_;
  TaskManager* task_manager_;
  WorkerResourceList resources_;
  // Map from worker process id to the list of its workers. This list contains
  // entries for worker processes which has not launched yet but for which we
  // have already received WorkerCreated event. We don't add such workers to
  // the task manager until the process is launched.
  ProcessIdToWorkerResources launching_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_WORKER_RESOURCE_PROVIDER_H_
