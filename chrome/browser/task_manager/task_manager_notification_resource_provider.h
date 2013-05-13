// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"

class BalloonHost;

class TaskManagerNotificationResource : public TaskManager::Resource {
 public:
  explicit TaskManagerNotificationResource(BalloonHost* balloon_host);
  virtual ~TaskManagerNotificationResource();

  // TaskManager::Resource interface
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

 private:
  // The icon painted for notifications.       .
  static gfx::ImageSkia* default_icon_;

  // Non-owned pointer to the balloon host.
  BalloonHost* balloon_host_;

  // Cached data about the balloon host.
  base::ProcessHandle process_handle_;
  int pid_;
  int unique_process_id_;
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerNotificationResource);
};

class TaskManagerNotificationResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  static TaskManagerNotificationResourceProvider* Create(
      TaskManager* task_manager);

  // TaskManager::ResourceProvider interface
  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  explicit TaskManagerNotificationResourceProvider(TaskManager* task_manager);
  virtual ~TaskManagerNotificationResourceProvider();

  void AddToTaskManager(BalloonHost* balloon_host);
  void RemoveFromTaskManager(BalloonHost* balloon_host);

  TaskManager* task_manager_;

  // Maps the actual resources (BalloonHost*) to the Task Manager resources.
  std::map<BalloonHost*, TaskManagerNotificationResource*> resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerNotificationResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_
