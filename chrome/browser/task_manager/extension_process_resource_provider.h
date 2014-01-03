// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_

#include <map>

#include "base/basictypes.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TaskManager;

namespace content {
class RenderViewHost;
}

namespace task_manager {

class ExtensionProcessResource;

class ExtensionProcessResourceProvider
    : public ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit ExtensionProcessResourceProvider(TaskManager* task_manager);

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
  virtual ~ExtensionProcessResourceProvider();

  bool IsHandledByThisProvider(content::RenderViewHost* render_view_host);
  void AddToTaskManager(content::RenderViewHost* render_view_host);
  void RemoveFromTaskManager(content::RenderViewHost* render_view_host);

  TaskManager* task_manager_;

  // Maps the actual resources (content::RenderViewHost*) to the Task Manager
  // resources.
  typedef std::map<content::RenderViewHost*, ExtensionProcessResource*>
      ExtensionRenderViewHostMap;
  ExtensionRenderViewHostMap resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_EXTENSION_PROCESS_RESOURCE_PROVIDER_H_
