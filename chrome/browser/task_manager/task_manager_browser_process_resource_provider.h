// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSER_PROCESS_RESOURCE_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSER_PROCESS_RESOURCE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/common/process_type.h"

namespace content {
class RenderViewHost;
class WebContents;
}

namespace extensions {
class Extension;
}

class TaskManagerBrowserProcessResource : public TaskManager::Resource {
 public:
  TaskManagerBrowserProcessResource();
  virtual ~TaskManagerBrowserProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;

  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;

  virtual bool ReportsSqliteMemoryUsed() const OVERRIDE;
  virtual size_t SqliteMemoryUsedBytes() const OVERRIDE;

  virtual bool ReportsV8MemoryStats() const OVERRIDE;
  virtual size_t GetV8MemoryAllocated() const OVERRIDE;
  virtual size_t GetV8MemoryUsed() const OVERRIDE;

 private:
  base::ProcessHandle process_;
  mutable string16 title_;

  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResource);
};

class TaskManagerBrowserProcessResourceProvider
    : public TaskManager::ResourceProvider {
 public:
  explicit TaskManagerBrowserProcessResourceProvider(
      TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

 private:
  virtual ~TaskManagerBrowserProcessResourceProvider();

  TaskManager* task_manager_;
  TaskManagerBrowserProcessResource resource_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_BROWSER_PROCESS_RESOURCE_PROVIDER_H_
