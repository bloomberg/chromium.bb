// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/process_type.h"
#include "ui/gfx/image/image_skia.h"

class TaskManagerChildProcessResource : public TaskManager::Resource {
 public:
  TaskManagerChildProcessResource(int process_type,
                                  const string16& name,
                                  base::ProcessHandle handle,
                                  int unique_process_id);
  virtual ~TaskManagerChildProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plug-in: Flash" when name is "Flash".
  string16 GetLocalizedTitle() const;

  int process_type_;
  string16 name_;
  base::ProcessHandle handle_;
  int pid_;
  int unique_process_id_;
  mutable string16 title_;
  bool network_usage_support_;

  // The icon painted for the child processs.
  // TODO(jcampan): we should have plugin specific icons for well-known
  // plugins.
  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResource);
};

class TaskManagerChildProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public content::BrowserChildProcessObserver {
 public:
  explicit TaskManagerChildProcessResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::BrowserChildProcessObserver methods:
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;

 private:
  virtual ~TaskManagerChildProcessResourceProvider();

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
  typedef std::map<base::ProcessHandle, TaskManagerChildProcessResource*>
      ChildProcessMap;
  ChildProcessMap resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  typedef std::map<int, TaskManagerChildProcessResource*> PidResourceMap;
  PidResourceMap pid_to_resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_CHILD_PROCESS_RESOURCE_PROVIDER_H_
