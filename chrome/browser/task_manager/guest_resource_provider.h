// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_GUEST_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_GUEST_RESOURCE_PROVIDER_H_

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

class GuestResource;

class GuestResourceProvider : public ResourceProvider,
                              public content::NotificationObserver {
 public:
  explicit GuestResourceProvider(TaskManager* task_manager);

  // ResourceProvider methods:
  virtual Resource* GetResource(int origin_pid,
                                int render_process_host_id,
                                int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~GuestResourceProvider();

  void Add(content::RenderViewHost* render_view_host);
  void Remove(content::RenderViewHost* render_view_host);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  typedef std::map<content::RenderViewHost*, GuestResource*> GuestResourceMap;
  GuestResourceMap resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GuestResourceProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_GUEST_RESOURCE_PROVIDER_H_
