// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/common/child_process_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class BackgroundContents;
class BalloonHost;
class Extension;
class ExtensionHost;
class RenderViewHost;
class TabContentsWrapper;

// These file contains the resource providers used in the task manager.

// Base class for various types of render process resources that provides common
// functionality like stats tracking.
class TaskManagerRendererResource : public TaskManager::Resource {
 public:
  TaskManagerRendererResource(base::ProcessHandle process,
                              RenderViewHost* render_view_host);
  virtual ~TaskManagerRendererResource();

  // TaskManager::Resource methods:
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual int GetRoutingId() const OVERRIDE;

  virtual bool ReportsCacheStats() const OVERRIDE;
  virtual WebKit::WebCache::ResourceTypeStats GetWebCoreCacheStats() const
      OVERRIDE;
  virtual bool ReportsFPS() const OVERRIDE;
  virtual float GetFPS() const OVERRIDE;
  virtual bool ReportsV8MemoryStats() const OVERRIDE;
  virtual size_t GetV8MemoryAllocated() const OVERRIDE;
  virtual size_t GetV8MemoryUsed() const OVERRIDE;

  // RenderResources are always inspectable.
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;

  // RenderResources always provide the network usage.
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

  virtual void Refresh() OVERRIDE;

  virtual void NotifyResourceTypeStats(
      const WebKit::WebCache::ResourceTypeStats& stats) OVERRIDE;

  virtual void NotifyFPS(float fps) OVERRIDE;

  virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                 size_t v8_memory_used) OVERRIDE;

 private:
  base::ProcessHandle process_;
  int pid_;

  // RenderViewHost we use to fetch stats.
  RenderViewHost* render_view_host_;
  // The stats_ field holds information about resource usage in the renderer
  // process and so it is updated asynchronously by the Refresh() call.
  WebKit::WebCache::ResourceTypeStats stats_;
  // This flag is true if we are waiting for the renderer to report its stats.
  bool pending_stats_update_;

  // The fps_ field holds the renderer frames per second.
  float fps_;
  // This flag is true if we are waiting for the renderer to report its FPS.
  bool pending_fps_update_;

  // We do a similar dance to gather the V8 memory usage in a process.
  size_t v8_memory_allocated_;
  size_t v8_memory_used_;
  bool pending_v8_memory_allocated_update_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerRendererResource);
};

class TaskManagerTabContentsResource : public TaskManagerRendererResource {
 public:
  explicit TaskManagerTabContentsResource(TabContentsWrapper* tab_contents);
  virtual ~TaskManagerTabContentsResource();

  // TaskManager::Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual TabContentsWrapper* GetTabContents() const OVERRIDE;
  virtual const Extension* GetExtension() const OVERRIDE;

 private:
  bool IsPrerendering() const;

  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  static SkBitmap* prerender_icon_;
  TabContentsWrapper* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResource);
};

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

  void Add(TabContentsWrapper* tab_contents);
  void Remove(TabContentsWrapper* tab_contents);

  void AddToTaskManager(TabContentsWrapper* tab_contents);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the TabContentsWrappers) to the Task Manager
  // resources.
  std::map<TabContentsWrapper*, TaskManagerTabContentsResource*> resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResourceProvider);
};

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
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual bool IsBackground() const OVERRIDE;

  const string16& application_name() const { return application_name_; }
 private:
  BackgroundContents* background_contents_;

  string16 application_name_;

  // The icon painted for BackgroundContents.
  // TODO(atwilson): Use the favicon when there's a way to get the favicon for
  // BackgroundContents.
  static SkBitmap* default_icon_;

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

class TaskManagerChildProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerChildProcessResource(const ChildProcessInfo& child_proc);
  virtual ~TaskManagerChildProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plug-in: Flash" when name is "Flash".
  string16 GetLocalizedTitle() const;

  ChildProcessInfo child_process_;
  int pid_;
  mutable string16 title_;
  bool network_usage_support_;

  // The icon painted for the child processs.
  // TODO(jcampan): we should have plugin specific icons for well-known
  // plugins.
  static SkBitmap* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResource);
};

class TaskManagerChildProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerChildProcessResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  void Add(const ChildProcessInfo& child_process_info);
  void Remove(const ChildProcessInfo& child_process_info);

  void AddToTaskManager(const ChildProcessInfo& child_process_info);

  TaskManager* task_manager_;

  // Maps the actual resources (the ChildProcessInfo) to the Task Manager
  // resources.
  std::map<ChildProcessInfo, TaskManagerChildProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerChildProcessResource*> pid_to_resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResourceProvider);
};

class TaskManagerExtensionProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerExtensionProcessResource(ExtensionHost* extension_host);
  virtual ~TaskManagerExtensionProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;
  virtual const Extension* GetExtension() const OVERRIDE;

  // Returns the pid of the extension process.
  int process_id() const { return pid_; }

  // Returns true if the associated extension has a background page.
  virtual bool IsBackground() const OVERRIDE;

 private:
  // The icon painted for the extension process.
  static SkBitmap* default_icon_;

  ExtensionHost* extension_host_;

  // Cached data about the extension.
  base::ProcessHandle process_handle_;
  int pid_;
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

  void AddToTaskManager(ExtensionHost* extension_host);
  void RemoveFromTaskManager(ExtensionHost* extension_host);

  TaskManager* task_manager_;

  // Maps the actual resources (ExtensionHost*) to the Task Manager resources.
  std::map<ExtensionHost*, TaskManagerExtensionProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerExtensionProcessResource*> pid_to_resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  bool updating_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerExtensionProcessResourceProvider);
};

class TaskManagerNotificationResource : public TaskManager::Resource {
 public:
  explicit TaskManagerNotificationResource(BalloonHost* balloon_host);
  virtual ~TaskManagerNotificationResource();

  // TaskManager::Resource interface
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

 private:
  // The icon painted for notifications.       .
  static SkBitmap* default_icon_;

  // Non-owned pointer to the balloon host.
  BalloonHost* balloon_host_;

  // Cached data about the balloon host.
  base::ProcessHandle process_handle_;
  int pid_;
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

class TaskManagerBrowserProcessResource : public TaskManager::Resource {
 public:
  TaskManagerBrowserProcessResource();
  virtual ~TaskManagerBrowserProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual SkBitmap GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
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
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

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

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
