// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/service_worker/service_worker_status_code.h"

class GURL;

namespace content {

class BrowserContext;
class SiteInstance;
class StoragePartitionImpl;

// Interacts with the UI thread to keep RenderProcessHosts alive while the
// ServiceWorker system is using them. It also tracks candidate processes
// for each pattern. Each instance of ServiceWorkerProcessManager is destroyed
// on the UI thread shortly after its ServiceWorkerContextWrapper is destroyed.
class CONTENT_EXPORT ServiceWorkerProcessManager {
 public:
  // The return value for AllocateWorkerProcess().
  struct AllocatedProcessInfo {
    // Same as RenderProcessHost::GetID().
    int process_id;

    // This must be one of NEW_PROCESS, EXISTING_UNREADY_PROCESS or
    // EXISTING_READY_PROCESS.
    ServiceWorkerMetrics::StartSituation start_situation;
  };

  // |*this| must be owned by a ServiceWorkerContextWrapper in a
  // StoragePartition within |browser_context|.
  explicit ServiceWorkerProcessManager(BrowserContext* browser_context);

  // Shutdown must be called before the ProcessManager is destroyed.
  ~ServiceWorkerProcessManager();

  // Called on the UI thread.
  BrowserContext* browser_context();

  // Synchronously prevents new processes from being allocated
  // and drops references to RenderProcessHosts. Called on the UI thread.
  void Shutdown();

  // Returns true if Shutdown() has been called. May be called by any thread.
  bool IsShutdown();

  // Returns a reference to a running process suitable for starting the service
  // worker described by |emdedded_worker_id|, |pattern|, and |script_url|.
  //
  // AllocateWorkerProcess() tries to return a process that already exists in
  // this ServiceWorkerProcessManager's map of processes, populated via
  // AddProcessReferenceToPattern().  If one is not found, or
  // |can_use_existing_process| is false, it will instead use SiteInstance to
  // get a process, possibly creating a new one.
  //
  // If SERVICE_WORKER_OK is returned, |out_info| contains information about the
  // process.
  //
  // Allocation can fail with SERVICE_WORKER_PROCESS_NOT_FOUND if
  // RenderProcessHost::Init fails.
  //
  // Called on the UI thread.
  ServiceWorkerStatusCode AllocateWorkerProcess(int embedded_worker_id,
                                                const GURL& pattern,
                                                const GURL& script_url,
                                                bool can_use_existing_process,
                                                AllocatedProcessInfo* out_info);

  // Drops a reference to a process that was running a Service Worker, and its
  // SiteInstance.  This must match a call to AllocateWorkerProcess.
  //
  // Called on the UI thread.
  void ReleaseWorkerProcess(int embedded_worker_id);

  // Sets a single process ID that will be used for all embedded workers.  This
  // bypasses the work of creating a process and managing its worker refcount so
  // that unittests can run without a BrowserContext.  The test is in charge of
  // making sure this is only called on the same thread as runs the UI message
  // loop.
  void SetProcessIdForTest(int process_id) {
    process_id_for_test_ = process_id;
  }

  // Sets the process ID to be used for tests that force creating a new process.
  void SetNewProcessIdForTest(int process_id) {
    new_process_id_for_test_ = process_id;
  }

  // Adds/removes process reference for the |pattern|, the process with highest
  // references count will be chosen to start a worker.
  void AddProcessReferenceToPattern(const GURL& pattern, int process_id);
  void RemoveProcessReferenceFromPattern(const GURL& pattern, int process_id);

  // Returns true if the |pattern| has at least one process to run.
  bool PatternHasProcessToRun(const GURL& pattern) const;

  // AsWeakPtr() can be called from any thread, but the WeakPtr must be
  // dereferenced on the UI thread only.
  base::WeakPtr<ServiceWorkerProcessManager> AsWeakPtr() { return weak_this_; }

  void set_storage_partition(StoragePartitionImpl* storage_partition) {
    storage_partition_ = storage_partition;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProcessManagerTest, SortProcess);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProcessManagerTest,
                           FindAvailableProcess);
  FRIEND_TEST_ALL_PREFIXES(
      ServiceWorkerProcessManagerTest,
      AllocateWorkerProcess_FindAvailableProcess_NonPlzNavigate);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProcessManagerTest,
                           AllocateWorkerProcess_WithProcessReuse);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProcessManagerTest,
                           AllocateWorkerProcess_WithoutProcessReuse);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProcessManagerTest,
                           AllocateWorkerProcess_InShutdown);

  // Information about the process for an EmbeddedWorkerInstance.
  struct ProcessInfo {
    explicit ProcessInfo(const scoped_refptr<SiteInstance>& site_instance);
    explicit ProcessInfo(int process_id);
    ProcessInfo(const ProcessInfo& other);
    ~ProcessInfo();

    // Stores the SiteInstance the Worker lives inside. This needs to outlive
    // the instance's use of its RPH to uphold assumptions in the
    // ContentBrowserClient interface.
    scoped_refptr<SiteInstance> site_instance;

    // In case the process was allocated without using a SiteInstance, we need
    // to store a process ID to decrement a worker reference on shutdown.
    // TODO(jyasskin): Implement http://crbug.com/372045 or thread a frame_id in
    // so all processes can be allocated with a SiteInstance.
    int process_id;
  };

  // Maps the process ID to its reference count.
  typedef std::map<int, int> ProcessRefMap;

  // Maps registration scope pattern to ProcessRefMap.
  typedef std::map<const GURL, ProcessRefMap> PatternProcessRefMap;

  // Returns a process vector sorted by the reference count for the |pattern|.
  std::vector<int> SortProcessesForPattern(const GURL& pattern) const;

  // Returns the id of an available process for this pattern, or
  // ChildProcessHost::kInvalidUniqueID if there is none.
  int FindAvailableProcess(const GURL& pattern);

  // Guarded by |browser_context_lock_|.
  // Written only on the UI thread, so the UI thread doesn't need to acquire the
  // lock when reading. Can be read from other threads with the lock.
  BrowserContext* browser_context_;

  // Protects |browser_context_|.
  base::Lock browser_context_lock_;

  //////////////////////////////////////////////////////////////////////////////
  // All fields below are only accessed on the UI thread.

  // May be null during initialization and in unit tests.
  StoragePartitionImpl* storage_partition_;

  // Maps the ID of a running EmbeddedWorkerInstance to information about the
  // process it's running inside. Since the Instances themselves live on the IO
  // thread, this can be slightly out of date:
  //  * instance_info_ is populated while an Instance is STARTING and before
  //    it's RUNNING.
  //  * instance_info_ is depopulated in a message sent as the Instance becomes
  //    STOPPED.
  std::map<int, ProcessInfo> instance_info_;

  // In unit tests, this will be returned as the process for all
  // EmbeddedWorkerInstances.
  int process_id_for_test_;
  int new_process_id_for_test_;

  // Candidate processes info for each pattern, should be accessed on the
  // UI thread.
  PatternProcessRefMap pattern_processes_;

  // Used to double-check that we don't access *this after it's destroyed.
  base::WeakPtr<ServiceWorkerProcessManager> weak_this_;
  base::WeakPtrFactory<ServiceWorkerProcessManager> weak_this_factory_;
};

}  // namespace content

namespace std {
// Specialized to post the deletion to the UI thread.
template <>
struct CONTENT_EXPORT default_delete<content::ServiceWorkerProcessManager> {
  void operator()(content::ServiceWorkerProcessManager* ptr) const;
};
}  // namespace std

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
