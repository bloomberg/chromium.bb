// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/common/shared_worker/shared_worker_connector.mojom.h"
#include "content/common/shared_worker/shared_worker_factory.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/worker_service.h"

namespace blink {
class MessagePortChannel;
}

namespace content {

class SharedWorkerInstance;
class SharedWorkerHost;
class SharedWorkerMessageFilter;
class ResourceContext;
class WorkerServiceObserver;
class WorkerStoragePartitionId;

// The implementation of WorkerService. We try to place workers in an existing
// renderer process when possible.
class CONTENT_EXPORT SharedWorkerServiceImpl : public WorkerService {
 public:
  // Returns the SharedWorkerServiceImpl singleton.
  static SharedWorkerServiceImpl* GetInstance();

  void AddFilter(SharedWorkerMessageFilter* filter);
  void RemoveFilter(SharedWorkerMessageFilter* filter);

  // WorkerService implementation:
  bool TerminateWorker(int process_id, int route_id) override;
  void TerminateAllWorkersForTesting(base::OnceClosure callback) override;
  std::vector<WorkerInfo> GetWorkers() override;
  void AddObserver(WorkerServiceObserver* observer) override;
  void RemoveObserver(WorkerServiceObserver* observer) override;

  // These methods correspond to worker related IPCs.
  void CreateWorker(
      int process_id,
      int frame_id,
      mojom::SharedWorkerInfoPtr info,
      mojom::SharedWorkerClientPtr client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      const blink::MessagePortChannel& port,
      ResourceContext* resource_context,
      const WorkerStoragePartitionId& partition_id);

  void OnProcessClosing(int process_id);

  // Checks the worker dependency of renderer processes and calls
  // IncrementWorkerRefCount and DecrementWorkerRefCount of
  // RenderProcessHostImpl on UI thread if necessary.
  void CheckWorkerDependency();

  void NotifyWorkerDestroyed(int process_id, int route_id);

  void DestroyHost(int process_id, int route_id);

 private:
  class SharedWorkerPendingInstance;
  class SharedWorkerReserver;

  friend struct base::DefaultSingletonTraits<SharedWorkerServiceImpl>;
  friend class SharedWorkerServiceImplTest;

  using UpdateWorkerDependencyFunc = void (*)(const std::vector<int>&,
                                              const std::vector<int>&);
  using TryReserveWorkerFunc = bool (*)(int /* process_id */,
                                        mojom::SharedWorkerFactoryPtrInfo*);

  using WorkerID = std::pair<int /* process_id */, int /* route_id */>;
  using WorkerHostMap = std::map<WorkerID, std::unique_ptr<SharedWorkerHost>>;
  using PendingInstanceMap =
      std::map<int, std::unique_ptr<SharedWorkerPendingInstance>>;

  SharedWorkerServiceImpl();
  ~SharedWorkerServiceImpl() override;

  void ResetForTesting();

  SharedWorkerMessageFilter* GetFilter(int render_process_id);

  // Reserves the render process to create Shared Worker. This reservation
  // procedure will be executed on UI thread and
  // RenderProcessReservedCallback() or RenderProcessReserveFailedCallback()
  // will be called on IO thread.
  // (SecureContextMismatch is used for UMA and should be handled as success.)
  void ReserveRenderProcessToCreateWorker(
      std::unique_ptr<SharedWorkerPendingInstance> pending_instance);

  // Called after the render process is reserved to create Shared Worker in it.
  void RenderProcessReservedCallback(int pending_instance_id,
                                     int worker_process_id,
                                     int worker_route_id,
                                     bool is_new_worker,
                                     mojom::SharedWorkerFactoryPtrInfo factory,
                                     bool pause_on_start);

  // Called after the fast shutdown is detected while reserving the render
  // process to create Shared Worker in it.
  void RenderProcessReserveFailedCallback(
      int pending_instance_id,
      int worker_process_id,
      int worker_route_id,
      bool is_new_worker,
      mojom::SharedWorkerFactoryPtrInfo factory,
      bool pause_on_start);

  // Returns nullptr if there is no host for given ids.
  SharedWorkerHost* FindSharedWorkerHost(int process_id, int route_id);

  SharedWorkerHost* FindSharedWorkerHost(const SharedWorkerInstance& instance);
  SharedWorkerPendingInstance* FindPendingInstance(
      const SharedWorkerInstance& instance);

  // Returns the IDs of the renderer processes which are executing
  // SharedWorkers connected to other renderer processes.
  const std::set<int> GetRenderersWithWorkerDependency();

  void ChangeUpdateWorkerDependencyFuncForTesting(
      UpdateWorkerDependencyFunc new_func);
  void ChangeTryReserveWorkerFuncForTesting(TryReserveWorkerFunc new_func);

  std::set<int> last_worker_depended_renderers_;
  // Function ptr to update worker dependency, tests may override this.
  UpdateWorkerDependencyFunc update_worker_dependency_;

  // Function ptr to reserve worker on UI thread, tests may override this.
  static TryReserveWorkerFunc s_try_reserve_worker_func_;

  WorkerHostMap worker_hosts_;
  PendingInstanceMap pending_instances_;
  int next_pending_instance_id_;

  base::ObserverList<WorkerServiceObserver> observers_;

  // Map from render process ID to filter.
  std::map<int, scoped_refptr<SharedWorkerMessageFilter>> filters_;

  base::OnceClosure terminate_all_workers_callback_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
