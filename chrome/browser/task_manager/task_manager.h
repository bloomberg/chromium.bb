// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "gpu/ipc/common/memory_stats.h"
#include "third_party/WebKit/public/web/WebCache.h"

class PrefRegistrySimple;
class PrivateWorkingSetSnapshot;
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

// This class is a singleton.
class TaskManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

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
  void AddResource(task_manager::Resource* resource);
  void RemoveResource(task_manager::Resource* resource);

  void OnWindowClosed();

  // Invoked when a change to a resource has occurred that should cause any
  // observers to completely refresh themselves (for example, the creation of
  // a background resource in a process). Results in all observers receiving
  // OnModelChanged() events.
  void ModelChanged();

  // Returns the singleton instance (and initializes it if necessary).
  static TaskManager* GetInstance();

  TaskManagerModel* model() const { return model_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, Resources);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, RefreshCalled);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest, Init);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest, Sort);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest,
                           SelectionAdaptsToSorting);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest,
                           EnsureNewPrimarySortColumn);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest,
                           EnsureOneColumnVisible);

  // Obtain an instance via GetInstance().
  TaskManager();
  friend struct base::DefaultSingletonTraits<TaskManager>;

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

  // Invoked when a range of items is to be immediately removed. It differs
  // from OnItemsRemoved by the fact that the item is still in the task manager,
  // so it can be queried for and found.
  virtual void OnItemsToBeRemoved(int start, int length) {}

  // Invoked when the initialization of the model has been finished and
  // periodical updates is started. The first periodical update will be done
  // in a few seconds. (depending on platform)
  virtual void OnReadyPeriodicalUpdate() {}
};

// The model used by TaskManager.
//
// TaskManagerModel caches the values from all task_manager::Resources. This is
// done so the UI sees a consistant view of the resources until it is told a
// value has been updated.
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
  int GetNaClDebugStubPort(int index) const;
  int64_t GetNetworkUsage(int index) const;
  double GetCPUUsage(int index) const;
  int GetIdleWakeupsPerSecond(int index) const;
  base::ProcessId GetProcessId(int index) const;
  base::ProcessHandle GetProcess(int index) const;

  // Catchall method that calls off to the appropriate GetResourceXXX method
  // based on |col_id|. |col_id| is an IDS_ value used to identify the column.
  base::string16 GetResourceById(int index, int col_id) const;

  // Methods to return formatted resource information.
  const base::string16& GetResourceTitle(int index) const;
  const base::string16& GetResourceProfileName(int index) const;
  base::string16 GetResourceNaClDebugStubPort(int index) const;
  base::string16 GetResourceNetworkUsage(int index) const;
  base::string16 GetResourceCPUUsage(int index) const;
  base::string16 GetResourcePrivateMemory(int index) const;
  base::string16 GetResourceSharedMemory(int index) const;
  base::string16 GetResourcePhysicalMemory(int index) const;
  base::string16 GetResourceProcessId(int index) const;
  base::string16 GetResourceGDIHandles(int index) const;
  base::string16 GetResourceUSERHandles(int index) const;
  base::string16 GetResourceWebCoreImageCacheSize(int index) const;
  base::string16 GetResourceWebCoreScriptsCacheSize(int index) const;
  base::string16 GetResourceWebCoreCSSCacheSize(int index) const;
  base::string16 GetResourceVideoMemory(int index) const;
  base::string16 GetResourceSqliteMemoryUsed(int index) const;
  base::string16 GetResourceIdleWakeupsPerSecond(int index) const;
  base::string16 GetResourceV8MemoryAllocatedSize(int index) const;

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

  // On Windows, get the current and peak number of GDI handles in use.
  void GetGDIHandles(int index, size_t* current, size_t* peak) const;

  // On Windows, get the current and peak number of USER handles in use.
  void GetUSERHandles(int index, size_t* current, size_t* peak) const;

  // Gets the statuses of webkit. Return false if the resource for the given row
  // isn't a renderer.
  bool GetWebCoreCacheStats(int index,
                            blink::WebCache::ResourceTypeStats* result) const;

  // Gets the GPU memory allocated of the given page.
  bool GetVideoMemory(int index,
                      size_t* video_memory,
                      bool* has_duplicates) const;

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

  // Returns true if the resource is first/last in its group (resources
  // rendered by the same process are groupped together).
  bool IsResourceFirstInGroup(int index) const;
  bool IsResourceLastInGroup(int index) const;

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

  // Returns the unique child process ID generated by Chromium, not the OS
  // process id. This is used to identify processes internally and for
  // extensions. It is not meant to be displayed to the user.
  int GetUniqueChildProcessId(int index) const;

  // Returns the type of the given resource.
  task_manager::Resource::Type GetResourceType(int index) const;

  // Returns WebContents of given resource or NULL if not applicable.
  content::WebContents* GetResourceWebContents(int index) const;

  void AddResource(task_manager::Resource* resource);
  void RemoveResource(task_manager::Resource* resource);

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

  // Updates the values for all rows.
  void Refresh();

  // Do a bulk repopulation of the physical_memory data on platforms where that
  // is faster.
  void RefreshPhysicalMemoryFromWorkingSetSnapshot();

  void NotifyVideoMemoryUsageStats(
      const gpu::VideoMemoryUsageStats& video_memory_usage_stats);

  void NotifyBytesRead(const net::URLRequest& request, int64_t bytes_read);

  void RegisterOnDataReadyCallback(const base::Closure& callback);

  void NotifyDataReady();

 private:
  friend class base::RefCountedThreadSafe<TaskManagerModel>;
  friend class TaskManagerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ProcessesApiTest, ProcessesVsTaskManager);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerTest, RefreshCalled);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerWindowControllerTest,
                           SelectionAdaptsToSorting);

  enum UpdateState {
    IDLE = 0,      // Currently not updating.
    TASK_PENDING,  // An update task is pending.
    STOPPING       // A update task is pending and it should stop the update.
  };

  // The delay between updates of the information (in ms).
#if defined(OS_MACOSX)
  // Match Activity Monitor's default refresh rate.
  static const int kUpdateTimeMs = 2000;
#else
  static const int kUpdateTimeMs = 1000;
#endif

  // Values cached per resource. Values are validated on demand. The is_XXX
  // members indicate if a value is valid.
  struct PerResourceValues {
    PerResourceValues();
    PerResourceValues(const PerResourceValues& other);
    ~PerResourceValues();

    bool is_title_valid;
    base::string16 title;

    bool is_profile_name_valid;
    base::string16 profile_name;

    // No is_network_usage since default (0) is fine.
    int64_t network_usage;

    bool is_process_id_valid;
    base::ProcessId process_id;

    bool is_webcore_stats_valid;
    blink::WebCache::ResourceTypeStats webcore_stats;

    bool is_sqlite_memory_bytes_valid;
    size_t sqlite_memory_bytes;

    bool is_v8_memory_valid;
    size_t v8_memory_allocated;
    size_t v8_memory_used;
  };

  // Values cached per process. Values are validated on demand. The is_XXX
  // members indicate if a value is valid.
  struct PerProcessValues {
    PerProcessValues();
    PerProcessValues(const PerProcessValues& other);
    ~PerProcessValues();

    bool is_cpu_usage_valid;
    double cpu_usage;

    bool is_idle_wakeups_valid;
    int idle_wakeups;

    bool is_private_and_shared_valid;
    size_t private_bytes;
    size_t shared_bytes;

    bool is_physical_memory_valid;
    size_t physical_memory;

    bool is_video_memory_valid;
    size_t video_memory;
    bool video_memory_has_duplicates;

    bool is_gdi_handles_valid;
    size_t gdi_handles;
    size_t gdi_handles_peak;

    bool is_user_handles_valid;
    size_t user_handles;
    size_t user_handles_peak;

    bool is_nacl_debug_stub_port_valid;
    int nacl_debug_stub_port;
  };

  typedef std::vector<task_manager::Resource*> ResourceList;
  typedef std::vector<scoped_refptr<task_manager::ResourceProvider> >
      ResourceProviderList;
  typedef std::map<base::ProcessHandle, ResourceList> GroupMap;
  typedef std::map<base::ProcessHandle, base::ProcessMetrics*> MetricsMap;
  typedef std::map<task_manager::Resource*, int64_t> ResourceValueMap;
  typedef std::map<task_manager::Resource*,
                   PerResourceValues> PerResourceCache;
  typedef std::map<base::ProcessHandle, PerProcessValues> PerProcessCache;

  // This struct is used to exchange information between the io and ui threads.
  struct BytesReadParam {
    BytesReadParam(int origin_pid,
                   int child_id,
                   int route_id,
                   int64_t byte_count)
        : origin_pid(origin_pid),
          child_id(child_id),
          route_id(route_id),
          byte_count(byte_count) {}

    // The process ID that triggered the request.  For plugin requests this
    // will differ from the renderer process ID.
    int origin_pid;

    // The child ID of the process this request was routed through.
    int child_id;

    int route_id;
    int64_t byte_count;
  };

  ~TaskManagerModel();

  // Callback from the timer to refresh. Invokes Refresh() as appropriate.
  void RefreshCallback();

  void RefreshVideoMemoryUsageStats();

  // Returns the network usage (in bytes per seconds) for the specified
  // resource. That's the value retrieved at the last timer's tick.
  int64_t GetNetworkUsageForResource(task_manager::Resource* resource) const;

  // Called on the UI thread when some bytes are read.
  void BytesRead(BytesReadParam param);

  void MultipleBytesRead(const std::vector<BytesReadParam>* params);

  // Notifies the UI thread about all the bytes read. Allows for coalescing
  // multiple bytes read into a single task for the UI thread. This is important
  // for when downloading a lot of data on the IO thread, since posting a Task
  // for each one is expensive.
  void NotifyMultipleBytesRead();

  // Called on the IO thread to start/stop updating byte counts.
  void SetUpdatingByteCount(bool is_updating);

  // Returns the network usage (in byte per second) that should be displayed for
  // the passed |resource|.  -1 means the information is not available for that
  // resource.
  int64_t GetNetworkUsage(task_manager::Resource* resource) const;

  // Returns the CPU usage (in %) that should be displayed for the passed
  // |resource|.
  double GetCPUUsage(task_manager::Resource* resource) const;

  // Returns the idle wakeups that should be displayed for the passed
  // |resource|.
  int GetIdleWakeupsPerSecond(task_manager::Resource* resource) const;

  // Given a number, this function returns the formatted string that should be
  // displayed in the task manager's memory cell.
  base::string16 GetMemCellText(int64_t number) const;

  // Verifies the private and shared memory for |handle| is valid in
  // |per_process_cache_|. Returns true if the data in |per_process_cache_| is
  // valid.
  bool CachePrivateAndSharedMemory(base::ProcessHandle handle) const;

  // Verifies |webcore_stats| in |per_resource_cache_|, returning true on
  // success.
  bool CacheWebCoreStats(int index) const;

  // Verifies |v8_memory_allocated| and |v8_memory_used| in
  // |per_resource_cache_|. Returns true if valid, false if not valid.
  bool CacheV8Memory(int index) const;

  // Adds a resource provider to be managed.
  void AddResourceProvider(task_manager::ResourceProvider* provider);

  // Returns the PerResourceValues for the specified index.
  PerResourceValues& GetPerResourceValues(int index) const;

  // Returns the Resource for the specified index.
  task_manager::Resource* GetResource(int index) const;

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

  // A map that contains the video memory usage for a process
  gpu::VideoMemoryUsageStats video_memory_usage_stats_;

  // Set to true when we've requested video stats and false once we get them.
  bool pending_video_memory_usage_stats_update_;

  // An observer waiting for video memory usage stats updates from the GPU
  // process
  std::unique_ptr<TaskManagerModelGpuDataManagerObserver>
      video_memory_usage_stats_observer_;

  base::ObserverList<TaskManagerModelObserver> observer_list_;

  // How many calls to StartUpdating have been made without matching calls to
  // StopUpdating.
  int update_requests_;

  // How many calls to StartListening have been made without matching calls to
  // StopListening.
  int listen_requests_;

  // Whether we are currently in the process of updating.
  UpdateState update_state_;

  // Whether the IO thread is currently in the process of updating; accessed
  // only on the IO thread.
  bool is_updating_byte_count_;

  // Buffer for coalescing BytesReadParam so we don't have to post a task on
  // each NotifyBytesRead() call.
  std::vector<BytesReadParam> bytes_read_buffer_;

  std::vector<base::Closure> on_data_ready_callbacks_;

#if defined(OS_WIN)
  std::unique_ptr<PrivateWorkingSetSnapshot> working_set_snapshot_;
#endif

  // All per-Resource values are stored here.
  mutable PerResourceCache per_resource_cache_;

  // All per-Process values are stored here.
  mutable PerProcessCache per_process_cache_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_H_
