// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_

#include <map>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/process_type.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class RenderViewHost;
}

namespace extensions {
class Extension;
}

class TaskManagerExtensionProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerExtensionProcessResource(
      content::RenderViewHost* render_view_host);
  virtual ~TaskManagerExtensionProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

  // Returns the pid of the extension process.
  int process_id() const { return pid_; }

  // Returns true if the associated extension has a background page.
  virtual bool IsBackground() const OVERRIDE;

 private:
  // The icon painted for the extension process.
  static gfx::ImageSkia* default_icon_;

  content::RenderViewHost* render_view_host_;

  // Cached data about the extension.
  base::ProcessHandle process_handle_;
  int pid_;
  int unique_process_id_;
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerExtensionProcessResource);
};

class TaskManagerExtensionProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerExtensionProcessResourceProvider(
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
  virtual ~TaskManagerExtensionProcessResourceProvider();

  bool IsHandledByThisProvider(content::RenderViewHost* render_view_host);
  void AddToTaskManager(content::RenderViewHost* render_view_host);
  void RemoveFromTaskManager(content::RenderViewHost* render_view_host);

  TaskManager* task_manager_;

  // Maps the actual resources (content::RenderViewHost*) to the Task Manager
  // resources.
  typedef std::map<content::RenderViewHost*,
      TaskManagerExtensionProcessResource*> ExtensionRenderViewHostMap;
  ExtensionRenderViewHostMap resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerExtensionProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_
