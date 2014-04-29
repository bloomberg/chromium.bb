// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"

class GURL;

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// Interacts with the UI thread to keep RenderProcessHosts alive while the
// ServiceWorker system is using them.  Each instance of
// ServiceWorkerProcessManager is destroyed on the UI thread shortly after its
// ServiceWorkerContextCore is destroyed on the IO thread.
class CONTENT_EXPORT ServiceWorkerProcessManager {
 public:
  // |*this| must be owned by |context_wrapper|->context().
  explicit ServiceWorkerProcessManager(
      ServiceWorkerContextWrapper* context_wrapper);

  ~ServiceWorkerProcessManager();

  // Returns a reference to a running process suitable for starting the Service
  // Worker at |script_url|. Processes in |process_ids| will be checked in order
  // for existence, and if none exist, then a new process will be created. Posts
  // |callback| to the IO thread to indicate whether creation succeeded and the
  // process ID that has a new reference.
  //
  // Allocation can fail with SERVICE_WORKER_ERROR_START_WORKER_FAILED if
  // RenderProcessHost::Init fails.
  void AllocateWorkerProcess(
      const std::vector<int>& process_ids,
      const GURL& script_url,
      const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
          callback) const;

  // Drops a reference to a process that was running a Service Worker.  This
  // must match a call to AllocateWorkerProcess.
  void ReleaseWorkerProcess(int process_id);

  // |increment_for_test| and |decrement_for_test| define how to look up a
  // process by ID and increment or decrement its worker reference count. This
  // must be called before any reference to this object escapes to another
  // thread, and is considered part of construction.
  void SetProcessRefcountOpsForTest(
      const base::Callback<bool(int)>& increment_for_test,
      const base::Callback<bool(int)>& decrement_for_test);

 private:
  bool IncrementWorkerRefcountByPid(int process_id) const;
  bool DecrementWorkerRefcountByPid(int process_id) const;

  // These fields are only accessed on the UI thread after construction.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  base::Callback<bool(int)> increment_for_test_;
  base::Callback<bool(int)> decrement_for_test_;

  // Used to double-check that we don't access *this after it's destroyed.
  base::WeakPtrFactory<ServiceWorkerProcessManager> weak_this_factory_;
  base::WeakPtr<ServiceWorkerProcessManager> weak_this_;
};

}  // namespace content

namespace base {
// Specialized to post the deletion to the UI thread.
template <>
struct CONTENT_EXPORT DefaultDeleter<content::ServiceWorkerProcessManager> {
  void operator()(content::ServiceWorkerProcessManager* ptr) const;
};
}  // namespace base

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
