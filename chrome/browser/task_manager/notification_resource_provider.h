// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BalloonHost;
class TaskManager;

namespace task_manager {

class NotificationResource;

class NotificationResourceProvider : public ResourceProvider,
                                     public content::NotificationObserver {
 public:
  static NotificationResourceProvider* Create(TaskManager* task_manager);

  // ResourceProvider interface
  virtual Resource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver interface
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  explicit NotificationResourceProvider(TaskManager* task_manager);
  virtual ~NotificationResourceProvider();

  void AddToTaskManager(BalloonHost* balloon_host);
  void RemoveFromTaskManager(BalloonHost* balloon_host);

  TaskManager* task_manager_;

  // Maps the actual resources (BalloonHost*) to the Task Manager resources.
  std::map<BalloonHost*, NotificationResource*> resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(NotificationResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_NOTIFICATION_RESOURCE_PROVIDER_H_
