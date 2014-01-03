// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_

#include <map>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BackgroundContents;
class TaskManager;

namespace task_manager {

class BackgroundContentsResource;

class BackgroundContentsResourceProvider
    : public ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit BackgroundContentsResourceProvider(TaskManager* task_manager);

  virtual Resource* GetResource(int origin_pid,
                                int child_id,
                                int route_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~BackgroundContentsResourceProvider();

  void Add(BackgroundContents* background_contents,
           const base::string16& title);
  void Remove(BackgroundContents* background_contents);

  void AddToTaskManager(BackgroundContents* background_contents,
                        const base::string16& title);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the BackgroundContents) to the Task Manager
  // resources.
  typedef std::map<BackgroundContents*, BackgroundContentsResource*>
      Resources;
  Resources resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_
