// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
#define CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/process_util.h"
#include "chrome/browser/task_manager.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"

class Extension;
class ExtensionHost;
class TabContents;

// These file contains the resource providers used in the task manager.

class TaskManagerTabContentsResource : public TaskManager::Resource {
 public:
  explicit TaskManagerTabContentsResource(TabContents* tab_contents);
  ~TaskManagerTabContentsResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  base::ProcessHandle GetProcess() const;
  TabContents* GetTabContents() const;

  virtual bool ReportsCacheStats() const { return true; }
  virtual WebKit::WebCache::ResourceTypeStats GetWebCoreCacheStats() const;

  virtual bool ReportsV8MemoryStats() const { return true; }
  virtual size_t GetV8MemoryAllocated() const;
  virtual size_t GetV8MemoryUsed() const;

  // TabContents always provide the network usage.
  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { }

  virtual void Refresh();

  virtual void NotifyResourceTypeStats(
      const WebKit::WebCache::ResourceTypeStats& stats);

  virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                 size_t v8_memory_used);

 private:
  TabContents* tab_contents_;
  base::ProcessHandle process_;
  int pid_;
  // The stats_ field holds information about resource usage in the renderer
  // process and so it is updated asynchronously by the Refresh() call.
  WebKit::WebCache::ResourceTypeStats stats_;
  // This flag is true if we are waiting for the renderer to report its stats.
  bool pending_stats_update_;

  // We do a similar dance to gather the V8 memory usage in a process.
  size_t v8_memory_allocated_;
  size_t v8_memory_used_;
  bool pending_v8_memory_allocated_update_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResource);
};

class TaskManagerTabContentsResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerTabContentsResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  virtual ~TaskManagerTabContentsResourceProvider();

  void Add(TabContents* tab_contents);
  void Remove(TabContents* tab_contents);

  void AddToTaskManager(TabContents* tab_contents);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the TabContents) to the Task Manager
  // resources.
  std::map<TabContents*, TaskManagerTabContentsResource*> resources_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResourceProvider);
};

class TaskManagerChildProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerChildProcessResource(ChildProcessInfo child_proc);
  ~TaskManagerChildProcessResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  base::ProcessHandle GetProcess() const;

  bool SupportNetworkUsage() const {
    return network_usage_support_;
  }

  void SetSupportNetworkUsage() {
    network_usage_support_ = true;
  }

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  ChildProcessInfo child_process_;
  int pid_;
  mutable std::wstring title_;
  bool network_usage_support_;

  // The icon painted for the child processs.
  // TODO(jcampan): we should have plugin specific icons for well-known
  // plugins.
  static SkBitmap* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResource);
};

class TaskManagerChildProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerChildProcessResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Retrieves the current ChildProcessInfo (performed in the IO thread).
  virtual void RetrieveChildProcessInfo();

  // Notifies the UI thread that the ChildProcessInfo have been retrieved.
  virtual void ChildProcessInfoRetreived();

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  // The list of ChildProcessInfo retrieved when starting the update.
  std::vector<ChildProcessInfo> existing_child_process_info_;

 private:
  virtual ~TaskManagerChildProcessResourceProvider();

  void Add(ChildProcessInfo child_process_info);
  void Remove(ChildProcessInfo child_process_info);

  void AddToTaskManager(ChildProcessInfo child_process_info);

  TaskManager* task_manager_;

  // Maps the actual resources (the ChildProcessInfo) to the Task Manager
  // resources.
  std::map<ChildProcessInfo, TaskManagerChildProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerChildProcessResource*> pid_to_resources_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResourceProvider);
};

class TaskManagerExtensionProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerExtensionProcessResource(ExtensionHost* extension_host);
  ~TaskManagerExtensionProcessResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  base::ProcessHandle GetProcess() const;
  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { NOTREACHED(); }
  const Extension* GetExtension() const;

  // Returns the pid of the extension process.
  int process_id() const { return pid_; }

 private:
  // The icon painted for the extension process.
  static SkBitmap* default_icon_;

  ExtensionHost* extension_host_;

  // Cached data about the extension.
  base::ProcessHandle process_handle_;
  int pid_;
  std::wstring title_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerExtensionProcessResource);
};

class TaskManagerExtensionProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerExtensionProcessResourceProvider(
      TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  virtual ~TaskManagerExtensionProcessResourceProvider();

  void AddToTaskManager(ExtensionHost* extension_host);
  void RemoveFromTaskManager(ExtensionHost* extension_host);

  TaskManager* task_manager_;

  // Maps the actual resources (ExtensionHost*) to the Task Manager resources.
  std::map<ExtensionHost*, TaskManagerExtensionProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerExtensionProcessResource*> pid_to_resources_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerExtensionProcessResourceProvider);
};

class TaskManagerBrowserProcessResource : public TaskManager::Resource {
 public:
  TaskManagerBrowserProcessResource();
  ~TaskManagerBrowserProcessResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  base::ProcessHandle GetProcess() const;

  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { NOTREACHED(); }

  bool ReportsSqliteMemoryUsed() const { return true; }
  size_t SqliteMemoryUsedBytes() const;

  // Returns the pid of the browser process.
  int process_id() const { return pid_; }

 private:
  base::ProcessHandle process_;
  int pid_;
  mutable std::wstring title_;

  static SkBitmap* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResource);
};

class TaskManagerBrowserProcessResourceProvider
    : public TaskManager::ResourceProvider {
 public:
  explicit TaskManagerBrowserProcessResourceProvider(
      TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

 private:
  virtual ~TaskManagerBrowserProcessResourceProvider();

  void AddToTaskManager(ChildProcessInfo child_process_info);

  TaskManager* task_manager_;
  TaskManagerBrowserProcessResource resource_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
