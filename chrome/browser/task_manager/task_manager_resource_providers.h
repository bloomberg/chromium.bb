// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/process_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class BackgroundContents;
class BalloonHost;
class Panel;
class Profile;

namespace content {
class RenderViewHost;
class WebContents;
}

namespace extensions {
class Extension;
}

// These file contains the resource providers used in the task manager.

// Base class for various types of render process resources that provides common
// functionality like stats tracking.
class TaskManagerRendererResource : public TaskManager::Resource {
 public:
  TaskManagerRendererResource(base::ProcessHandle process,
                              content::RenderViewHost* render_view_host);
  virtual ~TaskManagerRendererResource();

  // TaskManager::Resource methods:
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;

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

  content::RenderViewHost* render_view_host() const {
    return render_view_host_;
  }

 private:
  base::ProcessHandle process_;
  int pid_;
  int unique_process_id_;

  // RenderViewHost we use to fetch stats.
  content::RenderViewHost* render_view_host_;
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

// Tracks a single tab contents, prerendered page, instant page, or background
// printing page.
class TaskManagerTabContentsResource : public TaskManagerRendererResource {
 public:
  explicit TaskManagerTabContentsResource(content::WebContents* web_contents);
  virtual ~TaskManagerTabContentsResource();

  // Called when the underlying web_contents has been committed and is no
  // longer an Instant preview.
  void InstantCommitted();

  // TaskManager::Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  static gfx::ImageSkia* prerender_icon_;
  content::WebContents* web_contents_;
  Profile* profile_;
  bool is_instant_preview_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTabContentsResource);
};

// Provides resources for tab contents, prerendered pages, instant pages, and
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

class TaskManagerPanelResource : public TaskManagerRendererResource {
 public:
  explicit TaskManagerPanelResource(Panel* panel);
  virtual ~TaskManagerPanelResource();

  // TaskManager::Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  Panel* panel_;
  // Determines prefix for title reflecting whether extensions are apps
  // or in incognito mode.
  int message_prefix_id_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerPanelResource);
};

class TaskManagerPanelResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerPanelResourceProvider(TaskManager* task_manager);

  // TaskManager::ResourceProvider methods:
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
  virtual ~TaskManagerPanelResourceProvider();

  void Add(Panel* panel);
  void Remove(Panel* panel);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the Panels) to the Task Manager resources.
  typedef std::map<Panel*, TaskManagerPanelResource*> PanelResourceMap;
  PanelResourceMap resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerPanelResourceProvider);
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

class TaskManagerChildProcessResource : public TaskManager::Resource {
 public:
  TaskManagerChildProcessResource(content::ProcessType type,
                                  const string16& name,
                                  base::ProcessHandle handle,
                                  int unique_process_id);
  virtual ~TaskManagerChildProcessResource();

  // TaskManager::Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plug-in: Flash" when name is "Flash".
  string16 GetLocalizedTitle() const;

  content::ProcessType type_;
  string16 name_;
  base::ProcessHandle handle_;
  int pid_;
  int unique_process_id_;
  mutable string16 title_;
  bool network_usage_support_;

  // The icon painted for the child processs.
  // TODO(jcampan): we should have plugin specific icons for well-known
  // plugins.
  static gfx::ImageSkia* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResource);
};

class TaskManagerChildProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public content::BrowserChildProcessObserver {
 public:
  explicit TaskManagerChildProcessResourceProvider(TaskManager* task_manager);

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id) OVERRIDE;
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // content::BrowserChildProcessObserver methods:
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;

 private:
  virtual ~TaskManagerChildProcessResourceProvider();

  // Retrieves information about the running ChildProcessHosts (performed in the
  // IO thread).
  virtual void RetrieveChildProcessData();

  // Notifies the UI thread that the ChildProcessHosts information have been
  // retrieved.
  virtual void ChildProcessDataRetreived(
      const std::vector<content::ChildProcessData>& child_processes);

  void AddToTaskManager(const content::ChildProcessData& child_process_data);

  TaskManager* task_manager_;

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  // Maps the actual resources (the ChildProcessData) to the Task Manager
  // resources.
  typedef std::map<base::ProcessHandle, TaskManagerChildProcessResource*>
      ChildProcessMap;
  ChildProcessMap resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  typedef std::map<int, TaskManagerChildProcessResource*> PidResourceMap;
  PidResourceMap pid_to_resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResourceProvider);
};

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

class TaskManagerNotificationResource : public TaskManager::Resource {
 public:
  explicit TaskManagerNotificationResource(BalloonHost* balloon_host);
  virtual ~TaskManagerNotificationResource();

  // TaskManager::Resource interface
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

 private:
  // The icon painted for notifications.       .
  static gfx::ImageSkia* default_icon_;

  // Non-owned pointer to the balloon host.
  BalloonHost* balloon_host_;

  // Cached data about the balloon host.
  base::ProcessHandle process_handle_;
  int pid_;
  int unique_process_id_;
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


class TaskManagerGuestResource : public TaskManagerRendererResource {
 public:
  explicit TaskManagerGuestResource(content::RenderViewHost* render_view_host);
  virtual ~TaskManagerGuestResource();

  // TaskManager::Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerGuestResource);
};

class TaskManagerGuestResourceProvider
    : public TaskManager::ResourceProvider,
      public content::NotificationObserver {
 public:
  explicit TaskManagerGuestResourceProvider(TaskManager* task_manager);

  // TaskManager::ResourceProvider methods:
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
  virtual ~TaskManagerGuestResourceProvider();

  void Add(content::RenderViewHost* render_view_host);
  void Remove(content::RenderViewHost* render_view_host);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  typedef std::map<content::RenderViewHost*,
      TaskManagerGuestResource*> GuestResourceMap;
  GuestResourceMap resources_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerGuestResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
