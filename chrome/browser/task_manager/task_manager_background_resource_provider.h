// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_

#include <map>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_render_resource.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"

class BackgroundContents;

class TaskManagerBackgroundContentsResource
    : public TaskManagerRendererResource {
 public:
  TaskManagerBackgroundContentsResource(
      BackgroundContents* background_contents,
      const string16& application_name);
  virtual ~TaskManagerBackgroundContentsResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual bool IsBackground() const OVERRIDE;

  const string16& application_name() const { return application_name_; }
 private:
  BackgroundContents* background_contents_;

  string16 application_name_;

  // The icon painted for BackgroundContents.
  // TODO(atwilson): Use the favicon when there's a way to get the favicon for
  // BackgroundContents.
  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBackgroundContentsResource);
};

class TaskManagerBackgroundContentsResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerBackgroundContentsResourceProvider(
      TaskManager* task_manager);

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
  virtual ~TaskManagerBackgroundContentsResourceProvider();

  void Add(BackgroundContents* background_contents, const string16& title);
  void Remove(BackgroundContents* background_contents);

  void AddToTaskManager(BackgroundContents* background_contents,
                        const string16& title);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the BackgroundContents) to the Task Manager
  // resources.
  typedef std::map<BackgroundContents*, TaskManagerBackgroundContentsResource*>
      Resources;
  Resources resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBackgroundContentsResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BACKGROUND_RESOURCE_PROVIDER_H_
