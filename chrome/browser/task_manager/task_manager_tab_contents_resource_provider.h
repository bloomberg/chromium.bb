// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TAB_CONTENTS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TAB_CONTENTS_RESOURCE_PROVIDER_H_

#include <map>

#include "base/basictypes.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TaskManager;
class TaskManagerTabContentsResource;

namespace content {
class WebContents;
class NotificationSource;
class NotificationDetails;
}

// Provides resources for tab contents, prerendered pages, Instant pages, and
// background printing pages.
class TaskManagerTabContentsResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerTabContentsResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~TaskManagerTabContentsResourceProvider();

  void Add(content::WebContents* web_contents);
  void Remove(content::WebContents* web_contents);
  void InstantCommitted(content::WebContents* web_contents);

  void AddToTaskManager(content::WebContents* web_contents);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the WebContentses) to the Task Manager
  // resources.
  std::map<content::WebContents*, TaskManagerTabContentsResource*> resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TAB_CONTENTS_RESOURCE_PROVIDER_H_
