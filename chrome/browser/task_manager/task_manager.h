// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "net/url_request/url_request_job_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class Extension;
class SkBitmap;
class TabContents;
class TaskManagerModel;

namespace base {
class ProcessMetrics;
}

// This class is a singleton.
class TaskManager {
 public:
  // A resource represents one row in the task manager.
  // Resources from similar processes are grouped together by the task manager.
  class Resource {
   public:
    virtual ~Resource() {}

    enum Type {
      UNKNOWN = 0,     // An unknown process type.
      BROWSER,         // The main browser process.
      RENDERER,        // A normal TabContents renderer process.
      EXTENSION,       // An extension or app process.
      NOTIFICATION,    // A notification process.
      PLUGIN,          // A plugin process.
      WORKER,          // A web worker process.
      NACL,            // A NativeClient loader or broker process.
      UTILITY,         // A browser utility process.
      PROFILE_IMPORT,  // A profile import process.
      ZYGOTE,          // A Linux zygote process.
      SANDBOX_HELPER,  // A sandbox helper process.
      GPU              // A graphics process.
    };

    virtual string16 GetTitle() const = 0;
    virtual SkBitmap GetIcon() const = 0;
    virtual base::ProcessHandle GetProcess() const = 0;
    virtual Type GetType() const = 0;

    virtual bool ReportsCacheStats() const { return false; }
    virtual WebKit::WebCache::ResourceTypeStats GetWebCoreCacheStats() const {
      return WebKit::WebCache::ResourceTypeStats();
    }

    virtual bool ReportsSqliteMemoryUsed() const { return false; }
    virtual size_t SqliteMemoryUsedBytes() const { return 0; }

    // Return extension associated with the resource, or NULL
    // if not applicable.
    virtual const Extension* GetExtension() const { return NULL; }

    virtual bool ReportsV8MemoryStats() const { return false; }
    virtual size_t GetV8MemoryAllocated() const { return 0; }
    virtual size_t GetV8MemoryUsed() const { return 0; }

    // A helper function for ActivateFocusedTab.  Returns NULL by default
    // because not all resources have an associated tab.
    virtual TabContents* GetTabContents() const { return NULL; }

    // Whether this resource does report the network usage accurately.
    // This controls whether 0 or N/A is displayed when no bytes have been
    // reported as being read. This is because some plugins do not report the
    // bytes read and we don't want to display a misleading 0 value in that
    // case.
    virtual bool SupportNetworkUsage() const = 0;

    // Called when some bytes have been read and support_network_usage returns
    // false (meaning we do have network usage support).
    virtual void SetSupportNetworkUsage() = 0;

    // The TaskManagerModel periodically refreshes its data and call this
    // on all live resources.
    virtual void Refresh() {}

    virtual void NotifyResourceTypeStats(
        const WebKit::WebCache::ResourceTypeStats& stats) {}
    virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                   size_t v8_memory_used) {}

    // Returns true if this resource is not visible to the user because it lives
    // in the background (e.g. extension background page, background contents).
    virtual bool IsBackground() const { return false; }
  };

  // ResourceProviders are responsible for adding/removing resources to the task
  // manager. The task manager notifies the ResourceProvider that it is ready
  // to receive resource creation/termination notifications with a call to
  // StartUpdating(). At that point, the resource provider should call
  // AddResource with all the existing resources, and after that it should call
  // AddResource/RemoveResource as resources are created/terminated.
  // The provider remains the owner of the resource objects and is responsible
  // for deleting them (when StopUpdating() is called).
  // After StopUpdating() is called the provider should also stop reporting
  // notifications to the task manager.
  // Note: ResourceProviders have to be ref counted as they are used in
  // MessageLoop::InvokeLater().
  class ResourceProvider : public base::RefCountedThreadSafe<ResourceProvider> {
   public:
    // Should return the resource associated to the specified ids, or NULL if
    // the resource does not belong to this provider.
    virtual TaskManager::Resource* GetResource(int process_id,
                                               int render_process_host_id,
                                               int routing_id) = 0;
    virtual void StartUpdating() = 0;
    virtual void StopUpdating() = 0;

   protected:
    friend class base::RefCountedThreadSafe<ResourceProvider>;

    virtual ~ResourceProvider() {}
  };

  static void RegisterPrefs(PrefService* prefs);

  // Returns true if the process at the specified index is the browser process.
  bool IsBrowserProcess(int index) const;

  // Terminates the process at the specified index.
  void KillProcess(int index);

  // Activates the browser tab associated with the process in the specified
  // index.
  void ActivateProcess(int index);

  void AddResourceProvider(ResourceProvider* provider);
  void RemoveResourceProvider(ResourceProvider* provider);

  // These methods are invoked by the resource providers to add/remove resources
  // to the Task Manager. Note that the resources are owned by the
  // ResourceProviders and are not valid after StopUpdating() has been called
  // on the ResourceProviders.
  void AddResource(Resource* resource);
  void RemoveResource(Resource* resource);

  void OnWindowClosed();

  // Invoked when a change to a resource has occurred that should cause any
  // observers to completely refresh themselves (for example, the creation of
  // a background resource in a process). Results in all observers receiving
  // OnModelChanged() events.
  void ModelChanged();

  // Returns the singleton instance (and initializes it if necessary).
  static TaskManager* GetInstance();

  TaskManagerModel* model() const { return model_.get(); }

  void OpenAboutMemory();

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, Resources);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, RefreshCalled);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest, Init);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest, Sort);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest,
                           SelectionAdaptsToSorting);

  // Obtain an instance via GetInstance().
  TaskManager();
  friend struct DefaultSingletonTraits<TaskManager>;

  ~TaskManager();

  // The model used for gathering and processing task data. It is ref counted
  // because it is passed as a parameter to MessageLoop::InvokeLater().
  scoped_refptr<TaskManagerModel> model_;

  DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

class TaskManagerModelObserver {
 public:
  virtual ~TaskManagerModelObserver() {}

  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() = 0;

  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) = 0;

  // Invoked when new items are added.
  virtual void OnItemsAdded(int start, int length) = 0;

  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) = 0;
};

// The model that the TaskManager is using.
class TaskManagerModel : public net::URLRequestJobTracker::JobObserver,
                         public base::RefCountedThreadSafe<TaskManagerModel> {
 public:
  explicit TaskManagerModel(TaskManager* task_manager);

  void AddObserver(TaskManagerModelObserver* observer);
  void RemoveObserver(TaskManagerModelObserver* observer);

  // Returns number of registered resources.
  int ResourceCount() const;

  // Methods to return raw resource information.
  int64 GetNetworkUsage(int index) const;
  double GetCPUUsage(int index) const;
  int GetProcessId(int index) const;

  // Methods to return formatted resource information.
  string16 GetResourceTitle(int index) const;
  string16 GetResourceNetworkUsage(int index) const;
  string16 GetResourceCPUUsage(int index) const;
  string16 GetResourcePrivateMemory(int index) const;
  string16 GetResourceSharedMemory(int index) const;
  string16 GetResourcePhysicalMemory(int index) const;
  string16 GetResourceProcessId(int index) const;
  string16 GetResourceWebCoreImageCacheSize(int index) const;
  string16 GetResourceWebCoreScriptsCacheSize(int index) const;
  string16 GetResourceWebCoreCSSCacheSize(int index) const;
  string16 GetResourceSqliteMemoryUsed(int index) const;
  string16 GetResourceGoatsTeleported(int index) const;
  string16 GetResourceV8MemoryAllocatedSize(int index) const;

  // Gets the private memory (in bytes) that should be displayed for the passed
  // resource index. Caches the result since this calculation can take time on
  // some platforms.
  bool GetPrivateMemory(int index, size_t* result) const;

  // Gets the shared memory (in bytes) that should be displayed for the passed
  // resource index. Caches the result since this calculation can take time on
  // some platforms.
  bool GetSharedMemory(int index, size_t* result) const;

  // Gets the physical memory (in bytes) that should be displayed for the passed
  // resource index.
  bool GetPhysicalMemory(int index, size_t* result) const;

  // Gets the amount of memory allocated for javascript. Returns false if the
  // resource for the given row isn't a renderer.
  bool GetV8Memory(int index, size_t* result) const;

  // See design doc at http://go/at-teleporter for more information.
  int GetGoatsTeleported(int index) const;

  // Returns true if the resource is first in its group (resources
  // rendered by the same process are groupped together).
  bool IsResourceFirstInGroup(int index) const;

  // Returns true if the resource runs in the background (not visible to the
  // user, e.g. extension background pages and BackgroundContents).
  bool IsBackgroundResource(int index) const;

  // Returns icon to be used for resource (for example a favicon).
  SkBitmap GetResourceIcon(int index) const;

  // Returns a pair (start, length) of the group range of resource.
  std::pair<int, int> GetGroupRangeForResource(int index) const;

  // Compares values in column |col_id| and rows |row1|, |row2|.
  // Returns -1 if value in |row1| is less than value in |row2|,
  // 0 if they are equal, and 1 otherwise.
  int CompareValues(int row1, int row2, int col_id) const;

  // Returns process handle for given resource.
  base::ProcessHandle GetResourceProcessHandle(int index) const;

  // Returns the type of the given resource.
  TaskManager::Resource::Type GetResourceType(int index) const;

  // Returns TabContents of given resource or NULL if not applicable.
  TabContents* GetResourceTabContents(int index) const;

  // Returns Extension of given resource or NULL if not applicable.
  const Extension* GetResourceExtension(int index) const;

  // JobObserver methods:
  virtual void OnJobAdded(net::URLRequestJob* job);
  virtual void OnJobRemoved(net::URLRequestJob* job);
  virtual void OnJobDone(net::URLRequestJob* job,
                         const net::URLRequestStatus& status);
  virtual void OnJobRedirect(net::URLRequestJob* job,
                             const GURL& location,
                             int status_code);
  virtual void OnBytesRead(net::URLRequestJob* job,
                           const char* buf,
                           int byte_count);

  void AddResourceProvider(TaskManager::ResourceProvider* provider);
  void RemoveResourceProvider(TaskManager::ResourceProvider* provider);

  void AddResource(TaskManager::Resource* resource);
  void RemoveResource(TaskManager::Resource* resource);

  void StartUpdating();
  void StopUpdating();

  void Clear();  // Removes all items.

  // Sends OnModelChanged() to all observers to inform them of significant
  // changes to the model.
  void ModelChanged();

  void NotifyResourceTypeStats(
        base::ProcessId renderer_id,
        const WebKit::WebCache::ResourceTypeStats& stats);

  void NotifyV8HeapStats(base::ProcessId renderer_id,
                         size_t v8_memory_allocated,
                         size_t v8_memory_used);

 private:
  friend class base::RefCountedThreadSafe<TaskManagerModel>;
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, RefreshCalled);
  FRIEND_TEST_ALL_PREFIXES(ExtensionApiTest, ProcessesVsTaskManager);

  ~TaskManagerModel();

  enum UpdateState {
    IDLE = 0,      // Currently not updating.
    TASK_PENDING,  // An update task is pending.
    STOPPING       // A update task is pending and it should stop the update.
  };

  // This struct is used to exchange information between the io and ui threads.
  struct BytesReadParam {
    BytesReadParam(int origin_child_id,
                   int render_process_host_child_id,
                   int routing_id,
                   int byte_count)
        : origin_child_id(origin_child_id),
          render_process_host_child_id(render_process_host_child_id),
          routing_id(routing_id),
          byte_count(byte_count) {}

    // This is the child ID of the originator of the request. It will often be
    // the same as the render_process_host_child_id, but will be different when
    // another sub-process like a plugin is routing requests through a renderer.
    int origin_child_id;

    // The child ID of the RenderProcessHist this request was routed through.
    int render_process_host_child_id;

    int routing_id;
    int byte_count;
  };

  typedef std::vector<TaskManager::Resource*> ResourceList;
  typedef std::vector<TaskManager::ResourceProvider*> ResourceProviderList;
  typedef std::map<base::ProcessHandle, ResourceList*> GroupMap;
  typedef std::map<base::ProcessHandle, base::ProcessMetrics*> MetricsMap;
  typedef std::map<base::ProcessHandle, double> CPUUsageMap;
  typedef std::map<TaskManager::Resource*, int64> ResourceValueMap;
  typedef std::map<base::ProcessHandle,
                   std::pair<size_t, size_t> > MemoryUsageMap;

  // Updates the values for all rows.
  void Refresh();

  void AddItem(TaskManager::Resource* resource, bool notify_table);
  void RemoveItem(TaskManager::Resource* resource);

  // Register for network usage updates
  void RegisterForJobDoneNotifications();
  void UnregisterForJobDoneNotifications();

  // Returns the network usage (in bytes per seconds) for the specified
  // resource. That's the value retrieved at the last timer's tick.
  int64 GetNetworkUsageForResource(TaskManager::Resource* resource) const;

  // Called on the UI thread when some bytes are read.
  void BytesRead(BytesReadParam param);

  // Returns the network usage (in byte per second) that should be displayed for
  // the passed |resource|.  -1 means the information is not available for that
  // resource.
  int64 GetNetworkUsage(TaskManager::Resource* resource) const;

  // Returns the CPU usage (in %) that should be displayed for the passed
  // |resource|.
  double GetCPUUsage(TaskManager::Resource* resource) const;

  // Retrieves the ProcessMetrics for the resources at the specified row.
  // Returns true if there was a ProcessMetrics available.
  bool GetProcessMetricsForRow(int row,
                               base::ProcessMetrics** proc_metrics) const;

  // Given a number, this function returns the formatted string that should be
  // displayed in the task manager's memory cell.
  string16 GetMemCellText(int64 number) const;

  // Looks up the data for |handle| and puts it in the mutable cache
  // |memory_usage_map_|.
  bool GetAndCacheMemoryMetrics(base::ProcessHandle handle,
                                std::pair<size_t, size_t>* usage) const;

  // The list of providers to the task manager. They are ref counted.
  ResourceProviderList providers_;

  // The list of all the resources displayed in the task manager. They are owned
  // by the ResourceProviders.
  ResourceList resources_;

  // A map to keep tracks of the grouped resources (they are grouped if they
  // share the same process). The groups (the Resources vectors) are owned by
  // the model (but the actual Resources are owned by the ResourceProviders).
  GroupMap group_map_;

  // A map to retrieve the process metrics for a process. The ProcessMetrics are
  // owned by the model.
  MetricsMap metrics_map_;

  // A map that keeps track of the number of bytes read per process since last
  // tick. The Resources are owned by the ResourceProviders.
  ResourceValueMap current_byte_count_map_;

  // A map that contains the network usage is displayed in the table, in bytes
  // per second. It is computed every time the timer ticks. The Resources are
  // owned by the ResourceProviders.
  ResourceValueMap displayed_network_usage_map_;

  // A map that contains the CPU usage (in %) for a process since last refresh.
  CPUUsageMap cpu_usage_map_;

  // A map that contains the private/shared memory usage of the process. We
  // cache this because the same APIs are called on linux and windows, and
  // because the linux call takes >10ms to complete. This cache is cleared on
  // every Refresh().
  mutable MemoryUsageMap memory_usage_map_;

  ObserverList<TaskManagerModelObserver> observer_list_;

  // How many calls to StartUpdating have been made without matching calls to
  // StopUpdating.
  int update_requests_;

  // Whether we are currently in the process of updating.
  UpdateState update_state_;

  // A salt lick for the goats.
  int goat_salt_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
