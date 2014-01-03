// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_registrar.h"

class TaskManager;

namespace content {
struct ChildProcessData;
}

namespace task_manager {

class ChildProcessResource;

class ChildProcessResourceProvider
    : public ResourceProvider,
      public content::BrowserChildProcessObserver {
 public:
  explicit ChildProcessResourceProvider(TaskManager* task_manager);

  virtual Resource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::BrowserChildProcessObserver methods:
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;

 private:
  virtual ~ChildProcessResourceProvider();

  // Retrieves information about the running ChildProcessHosts (performed in the
  // IO thread).
  virtual void RetrieveChildProcessData();

  // Notifies the UI thread that the ChildProcessHosts information have been
  // retrieved.
  virtual void ChildProcessDataRetreived(
      const std::vector<content::ChildProcessData>& child_processes);

  void AddToTaskManager(const content::ChildProcessData& child_process_data);

  TaskManager* task_manager_;

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  // Maps the actual resources (the ChildProcessData) to the Task Manager
  // resources.
  typedef std::map<base::ProcessHandle, ChildProcessResource*>
      ChildProcessMap;
  ChildProcessMap resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  typedef std::map<int, ChildProcessResource*> PidResourceMap;
  PidResourceMap pid_to_resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_
