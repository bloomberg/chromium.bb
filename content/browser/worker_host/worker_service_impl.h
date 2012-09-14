// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/worker_service.h"

class GURL;
class WorkerStoragePartition;
struct ViewHostMsg_CreateWorker_Params;

namespace content {
class ResourceContext;
class WorkerServiceObserver;

class CONTENT_EXPORT WorkerServiceImpl
    : public NON_EXPORTED_BASE(WorkerService) {
 public:
  // Returns the WorkerServiceImpl singleton.
  static WorkerServiceImpl* GetInstance();

  // WorkerService implementation:
  virtual bool TerminateWorker(int process_id, int route_id) OVERRIDE;
  virtual std::vector<WorkerInfo> GetWorkers() OVERRIDE;
  virtual void AddObserver(WorkerServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(WorkerServiceObserver* observer) OVERRIDE;

  // These methods correspond to worker related IPCs.
  void CreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                    int route_id,
                    WorkerMessageFilter* filter,
                    ResourceContext* resource_context,
                    const WorkerStoragePartition& worker_partition);
  void LookupSharedWorker(const ViewHostMsg_CreateWorker_Params& params,
                          int route_id,
                          WorkerMessageFilter* filter,
                          ResourceContext* resource_context,
                          const WorkerStoragePartition& worker_partition,
                          bool* exists,
                          bool* url_error);
  void CancelCreateDedicatedWorker(int route_id, WorkerMessageFilter* filter);
  void ForwardToWorker(const IPC::Message& message,
                       WorkerMessageFilter* filter);
  void DocumentDetached(unsigned long long document_id,
                        WorkerMessageFilter* filter);

  void OnWorkerMessageFilterClosing(WorkerMessageFilter* filter);

  int next_worker_route_id() { return ++next_worker_route_id_; }

  // Given a worker's process id, return the IDs of the renderer process and
  // render view that created it.  For shared workers, this returns the first
  // parent.
  // TODO(dimich): This code assumes there is 1 worker per worker process, which
  // is how it is today until V8 can run in separate threads.
  bool GetRendererForWorker(int worker_process_id,
                            int* render_process_id,
                            int* render_view_id) const;
  const WorkerProcessHost::WorkerInstance* FindWorkerInstance(
      int worker_process_id);

  void NotifyWorkerDestroyed(
      WorkerProcessHost* process,
      int worker_route_id);

  // Used when we run each worker in a separate process.
  static const int kMaxWorkersWhenSeparate;
  static const int kMaxWorkersPerTabWhenSeparate;

 private:
  friend struct DefaultSingletonTraits<WorkerServiceImpl>;

  WorkerServiceImpl();
  virtual ~WorkerServiceImpl();

  // Given a WorkerInstance, create an associated worker process.
  bool CreateWorkerFromInstance(WorkerProcessHost::WorkerInstance instance);

  // Checks if we can create a worker process based on the process limit when
  // we're using a strategy of one process per core.
  bool CanCreateWorkerProcess(
      const WorkerProcessHost::WorkerInstance& instance);

  // Checks if the tab associated with the passed RenderView can create a
  // worker process based on the process limit when we're using a strategy of
  // one worker per process.
  bool TabCanCreateWorkerProcess(
      int render_process_id, int render_route_id, bool* hit_total_worker_limit);

  // Tries to see if any of the queued workers can be created.
  void TryStartingQueuedWorker();

  // APIs for manipulating our set of pending shared worker instances.
  WorkerProcessHost::WorkerInstance* CreatePendingInstance(
      const GURL& url,
      const string16& name,
      ResourceContext* resource_context,
      const WorkerStoragePartition& worker_partition);
  WorkerProcessHost::WorkerInstance* FindPendingInstance(
      const GURL& url,
      const string16& name,
      const WorkerStoragePartition& worker_partition,
      ResourceContext* resource_context);
  void RemovePendingInstances(
      const GURL& url,
      const string16& name,
      const WorkerStoragePartition& worker_partition,
      ResourceContext* resource_context);

  WorkerProcessHost::WorkerInstance* FindSharedWorkerInstance(
      const GURL& url,
      const string16& name,
      const WorkerStoragePartition& worker_partition,
      ResourceContext* resource_context);

  NotificationRegistrar registrar_;
  int next_worker_route_id_;

  WorkerProcessHost::Instances queued_workers_;

  // These are shared workers that have been looked up, but not created yet.
  // We need to keep a list of these to synchronously detect shared worker
  // URL mismatches when two pages launch shared workers simultaneously.
  WorkerProcessHost::Instances pending_shared_workers_;

  ObserverList<WorkerServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
