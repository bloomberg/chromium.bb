// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/common/gpu_memory_stats.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class PrefServiceSimple;
class TaskManagerModel;
class TaskManagerModelGpuDataManagerObserver;

namespace base {
class ProcessMetrics;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class ImageSkia;
}

namespace net {
class URLRequest;
}

#define TASKMANAGER_RESOURCE_TYPE_LIST(def) \
    def(BROWSER)         /* The main browser process. */ \
    def(RENDERER)        /* A normal WebContents renderer process. */ \
    def(EXTENSION)       /* An extension or app process. */ \
    def(NOTIFICATION)    /* A notification process. */ \
    def(GUEST)           /* A browser plugin guest process. */ \
    def(PLUGIN)          /* A plugin process. */ \
    def(WORKER)          /* A web worker process. */ \
    def(NACL)            /* A NativeClient loader or broker process. */ \
    def(UTILITY)         /* A browser utility process. */ \
    def(PROFILE_IMPORT)  /* A profile import process. */ \
    def(ZYGOTE)          /* A Linux zygote process. */ \
    def(SANDBOX_HELPER)  /* A sandbox helper process. */ \
    def(GPU)             /* A graphics process. */

#define TASKMANAGER_RESOURCE_TYPE_LIST_ENUM(a)   a,
#define TASKMANAGER_RESOURCE_TYPE_LIST_AS_STRING(a)   case a: return #a;

// This class is a singleton.
class TaskManager {
 public:
  // A resource represents one row in the task manager.
  // Resources from similar processes are grouped together by the task manager.
  class Resource {
   public:
    virtual ~Resource() {}

    enum Type {
      UNKNOWN = 0,
      TASKMANAGER_RESOURCE_TYPE_LIST(TASKMANAGER_RESOURCE_TYPE_LIST_ENUM)
    };

    virtual string16 GetTitle() const = 0;
    virtual string16 GetProfileName() const = 0;
    virtual gfx::ImageSkia GetIcon() const = 0;
    virtual base::ProcessHandle GetProcess() const = 0;
    virtual int GetUniqueChildProcessId() const = 0;
    virtual Type GetType() const = 0;
    virtual int GetRoutingID() const;

    virtual bool ReportsCacheStats() const;
    virtual WebKit::WebCache::ResourceTypeStats GetWebCoreCacheStats() const;

    virtual bool ReportsFPS() const;
    virtual float GetFPS() const;

    virtual bool ReportsSqliteMemoryUsed() const;
    virtual size_t SqliteMemoryUsedBytes() const;

    // Return extension associated with the resource, or NULL
    // if not applicable.
    virtual const extensions::Extension* GetExtension() const;

    virtual bool ReportsV8MemoryStats() const;
    virtual size_t GetV8MemoryAllocated() const;
    virtual size_t GetV8MemoryUsed() const;

    // Returns true if this resource can be inspected using developer tools.
    virtual bool CanInspect() const;

    // Invokes or reveals developer tools window for this resource.
    virtual void Inspect() const {}

    // A helper function for ActivateProcess when selected resource refers
    // to a Tab or other window containing web contents.  Returns NULL by
    // default because not all resources have an associated web contents.
    virtual content::WebContents* GetWebContents() const;

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
    virtual void NotifyFPS(float fps) {}
    virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                   size_t v8_memory_used) {}

    // Returns true if this resource is not visible to the user because it lives
    // in the background (e.g. extension background page, background contents).
    virtual bool IsBackground() const;

    static const char* GetResourceTypeAsString(const Type type) {
      switch (type) {
        TASKMANAGER_RESOURCE_TYPE_LIST(TASKMANAGER_RESOURCE_TYPE_LIST_AS_STRING)
        default: return "UNKNOWN";
      }
    }

    // Returns resource identifier that is unique within single task manager
    // session (between StartUpdating and StopUpdating).
    int get_unique_id() { return unique_id_; }

   protected:
    Resource() : unique_id_(0) {}

   private:
    friend class TaskManagerModel;
    int unique_id_;
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

  static void RegisterPrefs(PrefServiceSimple* prefs);

  // Returns true if the process at the specified index is the browser process.
  bool IsBrowserProcess(int index) const;

  // Terminates the process at the specified index.
  void KillProcess(int index);

  // Activates the browser tab associated with the process in the specified
  // index.
  void ActivateProcess(int index);

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

  void OpenAboutMemory(chrome::HostDesktopType desktop_type);

  // Returns the number of background pages that will be displayed in the
  // TaskManager. Used by the wrench menu code to display a count of background
  // pages in the "View Background Pages" menu item.
  static int GetBackgroundPageCount();

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

#undef TASKMANAGER_RESOURCE_TYPE_LIST
#undef DEFINE_ENUM
#undef DEFINE_CONVERT_TO_STRING


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

  // Invoked when a range of items is to be immediately removed. It differs
  // from OnItemsRemoved by the fact that the item is still in the task manager,
  // so it can be queried for and found.
  virtual void OnItemsToBeRemoved(int start, int length) {}

  // Invoked when the initialization of the model has been finished and
  // periodical updates is started. The first periodical update will be done
  // in a few seconds. (depending on platform)
  virtual void OnReadyPeriodicalUpdate() {}
};

// The model that the TaskManager is using.
class TaskManagerModel : public base::RefCountedThreadSafe<TaskManagerModel> {
 public:
  // (start, length)
  typedef std::pair<int, int> GroupRange;

  explicit TaskManagerModel(TaskManager* task_manager);

  void AddObserver(TaskManagerModelObserver* observer);
  void RemoveObserver(TaskManagerModelObserver* observer);

  // Returns number of registered resources.
  int ResourceCount() const;
  // Returns number of registered groups.
  int GroupCount() const;

  // Methods to return raw resource information.
  int64 GetNetworkUsage(int index) const;
  double GetCPUUsage(int index) const;
  int GetProcessId(int index) const;
  base::ProcessHandle GetProcess(int index) const;
  int GetResourceUniqueId(int index) const;
  // Returns the index of resource that has the given |unique_id|. Returns -1 if
  // no resouce has the |unique_id|.
  int GetResourceIndexByUniqueId(const int unique_id) const;

  // Catchall method that calls off to the appropriate GetResourceXXX method
  // based on |col_id|. |col_id| is an IDS_ value used to identify the column.
  string16 GetResourceById(int index, int col_id) const;

  // Methods to return formatted resource information.
  string16 GetResourceTitle(int index) const;
  string16 GetResourceProfileName(int index) const;
  string16 GetResourceNetworkUsage(int index) const;
  string16 GetResourceCPUUsage(int index) const;
  string16 GetResourcePrivateMemory(int index) const;
  string16 GetResourceSharedMemory(int index) const;
  string16 GetResourcePhysicalMemory(int index) const;
  string16 GetResourceProcessId(int index) const;
  string16 GetResourceWebCoreImageCacheSize(int index) const;
  string16 GetResourceWebCoreScriptsCacheSize(int index) const;
  string16 GetResourceWebCoreCSSCacheSize(int index) const;
  string16 GetResourceVideoMemory(int index) const;
  string16 GetResourceFPS(int index) const;
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

  // Gets the statuses of webkit. Return false if the resource for the given row
  // isn't a renderer.
  bool GetWebCoreCacheStats(int index,
                            WebKit::WebCache::ResourceTypeStats* result) const;

  // Gets the GPU memory allocated of the given page.
  bool GetVideoMemory(int index,
                      size_t* video_memory,
                      bool* has_duplicates) const;

  // Gets the fps of the given page. Return false if the resource for the given
  // row isn't a renderer.
  bool GetFPS(int index, float* result) const;

  // Gets the sqlite memory (in byte). Return false if the resource for the
  // given row doesn't report information.
  bool GetSqliteMemoryUsedBytes(int index, size_t* result) const;

  // Gets the amount of memory allocated for javascript. Returns false if the
  // resource for the given row isn't a renderer.
  bool GetV8Memory(int index, size_t* result) const;

  // Gets the amount of memory used for javascript. Returns false if the
  // resource for the given row isn't a renderer.
  bool GetV8MemoryUsed(int index, size_t* result) const;

  // Returns true if resource for the given row can be activated.
  bool CanActivate(int index) const;

  // Returns true if resource for the given row can be inspected using developer
  // tools.
  bool CanInspect(int index) const;

  // Invokes or reveals developer tools window for resource in the given row.
  void Inspect(int index) const;

  // See design doc at http://go/at-teleporter for more information.
  int GetGoatsTeleported(int index) const;

  // Returns true if the resource is first/last in its group (resources
  // rendered by the same process are groupped together).
  bool IsResourceFirstInGroup(int index) const;
  bool IsResourceLastInGroup(int index) const;

  // Returns true if the resource runs in the background (not visible to the
  // user, e.g. extension background pages and BackgroundContents).
  bool IsBackgroundResource(int index) const;

  // Returns icon to be used for resource (for example a favicon).
  gfx::ImageSkia GetResourceIcon(int index) const;

  // Returns the group range of resource.
  GroupRange GetGroupRangeForResource(int index) const;

  // Returns an index of groups to which the resource belongs.
  int GetGroupIndexForResource(int index) const;

  // Returns an index of resource which belongs to the |group_index|th group
  // and which is the |index_in_group|th resource in group.
  int GetResourceIndexForGroup(int group_index, int index_in_group) const;

  // Compares values in column |col_id| and rows |row1|, |row2|.
  // Returns -1 if value in |row1| is less than value in |row2|,
  // 0 if they are equal, and 1 otherwise.
  int CompareValues(int row1, int row2, int col_id) const;

  // Returns process handle for given resource.
  base::ProcessHandle GetResourceProcessHandle(int index) const;

  // Returns the unique child process ID generated by Chromium, not the OS
  // process id. This is used to identify processes internally and for
  // extensions. It is not meant to be displayed to the user.
  int GetUniqueChildProcessId(int index) const;

  // Returns the type of the given resource.
  TaskManager::Resource::Type GetResourceType(int index) const;

  // Returns WebContents of given resource or NULL if not applicable.
  content::WebContents* GetResourceWebContents(int index) const;

  // Returns Extension of given resource or NULL if not applicable.
  const extensions::Extension* GetResourceExtension(int index) const;

  void AddResource(TaskManager::Resource* resource);
  void RemoveResource(TaskManager::Resource* resource);

  void StartUpdating();
  void StopUpdating();

  // Listening involves calling StartUpdating on all resource providers. This
  // causes all of them to subscribe to notifications and enumerate current
  // resources. It differs from StartUpdating that it doesn't start the
  // Refresh timer. The end result is that we have a full view of resources, but
  // don't spend unneeded time updating, unless we have a real need to.
  void StartListening();
  void StopListening();

  void Clear();  // Removes all items.

  // Sends OnModelChanged() to all observers to inform them of significant
  // changes to the model.
  void ModelChanged();

  void NotifyResourceTypeStats(
        base::ProcessId renderer_id,
        const WebKit::WebCache::ResourceTypeStats& stats);

  void NotifyFPS(base::ProcessId renderer_id,
                 int routing_id,
                 float fps);

  void NotifyVideoMemoryUsageStats(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats);

  void NotifyV8HeapStats(base::ProcessId renderer_id,
                         size_t v8_memory_allocated,
                         size_t v8_memory_used);

  void NotifyBytesRead(const net::URLRequest& request, int bytes_read);

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
    BytesReadParam(int origin_pid,
                   int render_process_host_child_id,
                   int routing_id,
                   int byte_count)
        : origin_pid(origin_pid),
          render_process_host_child_id(render_process_host_child_id),
          routing_id(routing_id),
          byte_count(byte_count) {}

    // The process ID that triggered the request.  For plugin requests this
    // will differ from the renderer process ID.
    int origin_pid;

    // The child ID of the RenderProcessHost this request was routed through.
    int render_process_host_child_id;

    int routing_id;
    int byte_count;
  };

  typedef std::vector<TaskManager::Resource*> ResourceList;
  typedef std::vector<scoped_refptr<TaskManager::ResourceProvider> >
      ResourceProviderList;
  typedef std::map<base::ProcessHandle, ResourceList*> GroupMap;
  typedef std::map<base::ProcessHandle, base::ProcessMetrics*> MetricsMap;
  typedef std::map<base::ProcessHandle, double> CPUUsageMap;
  typedef std::map<TaskManager::Resource*, int64> ResourceValueMap;
  // Private memory in bytes, shared memory in bytes.
  typedef std::pair<size_t, size_t> MemoryUsageEntry;
  typedef std::map<base::ProcessHandle, MemoryUsageEntry> MemoryUsageMap;

  // Updates the values for all rows.
  void Refresh();

  void RefreshVideoMemoryUsageStats();

  void AddItem(TaskManager::Resource* resource, bool notify_table);
  void RemoveItem(TaskManager::Resource* resource);

  // Returns the network usage (in bytes per seconds) for the specified
  // resource. That's the value retrieved at the last timer's tick.
  int64 GetNetworkUsageForResource(TaskManager::Resource* resource) const;

  // Called on the UI thread when some bytes are read.
  void BytesRead(BytesReadParam param);

  void MultipleBytesRead(const std::vector<BytesReadParam>* params);

  // Notifies the UI thread about all the bytes read. Allows for coalescing
  // multiple bytes read into a single task for the UI thread. This is important
  // for when downloading a lot of data on the IO thread, since posting a Task
  // for each one is expensive.
  void NotifyMultipleBytesRead();

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
                                MemoryUsageEntry* usage) const;

  // Adds a resource provider to be managed.
  void AddResourceProvider(TaskManager::ResourceProvider* provider);

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

  // A map that contains the video memory usage for a process
  content::GPUVideoMemoryUsageStats video_memory_usage_stats_;
  bool pending_video_memory_usage_stats_update_;

  // An observer waiting for video memory usage stats updates from the GPU
  // process
  scoped_ptr<TaskManagerModelGpuDataManagerObserver>
      video_memory_usage_stats_observer_;

  // A map that contains the private/shared memory usage of the process. We
  // cache this because the same APIs are called on linux and windows, and
  // because the linux call takes >10ms to complete. This cache is cleared on
  // every Refresh().
  mutable MemoryUsageMap memory_usage_map_;

  ObserverList<TaskManagerModelObserver> observer_list_;

  // How many calls to StartUpdating have been made without matching calls to
  // StopUpdating.
  int update_requests_;

  // How many calls to StartListening have been made without matching calls to
  // StopListening.
  int listen_requests_;

  // Whether we are currently in the process of updating.
  UpdateState update_state_;

  // A salt lick for the goats.
  uint64 goat_salt_;

  // Resource identifier that is unique within single session.
  int last_unique_id_;

  // Buffer for coalescing BytesReadParam so we don't have to post a task on
  // each NotifyBytesRead() call.
  std::vector<BytesReadParam> bytes_read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
