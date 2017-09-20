// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/common/shared_worker/shared_worker_client.mojom.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/worker_service_observer.h"

namespace content {

WorkerService* WorkerService::GetInstance() {
  return SharedWorkerServiceImpl::GetInstance();
}

bool IsHostAlive(RenderProcessHostImpl* host) {
  return host && !host->FastShutdownStarted() &&
         !host->IsKeepAliveRefCountDisabled();
}

namespace {

// A helper to request a renderer process allocation for a shared worker to the
// UI thread from the IO thread.
class SharedWorkerReserver {
 public:
  ~SharedWorkerReserver() = default;

  using ResolverCallback = base::Callback<void(int worker_process_id,
                                               int worker_route_id,
                                               bool is_new_worker,
                                               bool pause_on_start)>;

  // Tries to reserve a renderer process for a shared worker. This should be
  // called on the IO thread and given callbacks are also invoked on the IO
  // thread on completion.
  static void TryReserve(int worker_process_id,
                         int worker_route_id,
                         bool is_new_worker,
                         const SharedWorkerInstance& instance,
                         const ResolverCallback& success_cb,
                         const ResolverCallback& failure_cb,
                         bool (*try_increment_worker_ref_count)(int)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    std::unique_ptr<SharedWorkerReserver> reserver(new SharedWorkerReserver(
        worker_process_id, worker_route_id, is_new_worker, instance));
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&SharedWorkerReserver::TryReserveOnUI,
                       std::move(reserver), success_cb, failure_cb,
                       try_increment_worker_ref_count));
  }

 private:
  SharedWorkerReserver(int worker_process_id,
                       int worker_route_id,
                       bool is_new_worker,
                       const SharedWorkerInstance& instance)
      : worker_process_id_(worker_process_id),
        worker_route_id_(worker_route_id),
        is_new_worker_(is_new_worker),
        instance_(instance) {}

  void TryReserveOnUI(const ResolverCallback& success_cb,
                      const ResolverCallback& failure_cb,
                      bool (*try_increment_worker_ref_count)(int)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    bool pause_on_start = false;
    if (!try_increment_worker_ref_count(worker_process_id_)) {
      DidTryReserveOnUI(failure_cb, pause_on_start);
      return;
    }
    if (is_new_worker_) {
      pause_on_start =
          SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
              worker_process_id_, worker_route_id_, instance_);
    }
    DidTryReserveOnUI(success_cb, pause_on_start);
  }

  void DidTryReserveOnUI(const ResolverCallback& callback,
                         bool pause_on_start) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, worker_process_id_, worker_route_id_,
                       is_new_worker_, pause_on_start));
  }

  const int worker_process_id_;
  const int worker_route_id_;
  const bool is_new_worker_;
  const SharedWorkerInstance instance_;
};

class ScopedWorkerDependencyChecker {
 public:
  explicit ScopedWorkerDependencyChecker(SharedWorkerServiceImpl* service)
      : service_(service) {}
  ScopedWorkerDependencyChecker(SharedWorkerServiceImpl* service,
                                base::Closure done_closure)
      : service_(service), done_closure_(done_closure) {}
  ~ScopedWorkerDependencyChecker() {
    service_->CheckWorkerDependency();
    if (!done_closure_.is_null())
      done_closure_.Run();
  }

 private:
  SharedWorkerServiceImpl* service_;
  base::Closure done_closure_;
  DISALLOW_COPY_AND_ASSIGN(ScopedWorkerDependencyChecker);
};

void UpdateWorkerDependencyOnUI(const std::vector<int>& added_ids,
                                const std::vector<int>& removed_ids) {
  for (int id : added_ids) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(RenderProcessHost::FromID(id));
    if (!IsHostAlive(render_process_host_impl))
      continue;
    render_process_host_impl->IncrementKeepAliveRefCount();
  }
  for (int id : removed_ids) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(RenderProcessHost::FromID(id));
    if (!IsHostAlive(render_process_host_impl))
      continue;
    render_process_host_impl->DecrementKeepAliveRefCount();
  }
}

void UpdateWorkerDependency(const std::vector<int>& added_ids,
                            const std::vector<int>& removed_ids) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&UpdateWorkerDependencyOnUI, added_ids, removed_ids));
}

void DecrementWorkerRefCount(int process_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&DecrementWorkerRefCount, process_id));
    return;
  }
  RenderProcessHostImpl* render_process_host_impl =
      static_cast<RenderProcessHostImpl*>(
          RenderProcessHost::FromID(process_id));
  if (IsHostAlive(render_process_host_impl))
    render_process_host_impl->DecrementKeepAliveRefCount();
}

bool TryIncrementWorkerRefCount(int worker_process_id) {
  RenderProcessHostImpl* render_process = static_cast<RenderProcessHostImpl*>(
      RenderProcessHost::FromID(worker_process_id));
  if (!IsHostAlive(render_process))
    return false;
  render_process->IncrementKeepAliveRefCount();
  return true;
}

}  // namespace

class SharedWorkerServiceImpl::SharedWorkerPendingInstance {
 public:
  struct SharedWorkerPendingRequest {
    SharedWorkerPendingRequest(
        int render_process_id,
        int render_frame_route_id,
        mojom::SharedWorkerClientPtr client,
        const MessagePort& message_port,
        blink::mojom::SharedWorkerCreationContextType creation_context_type)
        : render_process_id(render_process_id),
          render_frame_route_id(render_frame_route_id),
          client(std::move(client)),
          message_port(message_port),
          creation_context_type(creation_context_type) {}
    const int render_process_id;
    const int render_frame_route_id;
    mojom::SharedWorkerClientPtr client;
    MessagePort message_port;
    const blink::mojom::SharedWorkerCreationContextType creation_context_type;
  };

  using SharedWorkerPendingRequests =
      std::vector<std::unique_ptr<SharedWorkerPendingRequest>>;

  explicit SharedWorkerPendingInstance(
      std::unique_ptr<SharedWorkerInstance> instance)
      : instance_(std::move(instance)) {}

  ~SharedWorkerPendingInstance() {}

  SharedWorkerInstance* instance() { return instance_.get(); }
  SharedWorkerInstance* release_instance() { return instance_.release(); }
  SharedWorkerPendingRequests* requests() { return &requests_; }

  void AddRequest(std::unique_ptr<SharedWorkerPendingRequest> request_info) {
    requests_.push_back(std::move(request_info));
  }

  void RemoveRequest(int process_id) {
    auto to_remove = std::remove_if(
        requests_.begin(), requests_.end(),
        [process_id](const std::unique_ptr<SharedWorkerPendingRequest>& r) {
          return r->render_process_id == process_id;
        });
    requests_.erase(to_remove, requests_.end());
  }

  void RemoveRequestFromFrame(int process_id, int frame_id) {
    auto to_remove = std::remove_if(
        requests_.begin(), requests_.end(),
        [process_id,
         frame_id](const std::unique_ptr<SharedWorkerPendingRequest>& r) {
          return r->render_process_id == process_id &&
                 r->render_frame_route_id == frame_id;
        });
    requests_.erase(to_remove, requests_.end());
  }

  void RegisterToSharedWorkerHost(SharedWorkerHost* host) {
    for (const auto& request : requests_) {
      // Pass the actual creation context type, so the client can understand if
      // there is a mismatch between security levels.
      request->client->OnCreated(host->instance()->creation_context_type());

      host->AddClient(std::move(request->client), request->render_process_id,
                      request->render_frame_route_id, request->message_port);
    }
  }

 private:
  std::unique_ptr<SharedWorkerInstance> instance_;
  SharedWorkerPendingRequests requests_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerPendingInstance);
};

// static
bool (*SharedWorkerServiceImpl::s_try_increment_worker_ref_count_)(int) =
    TryIncrementWorkerRefCount;

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::Singleton<SharedWorkerServiceImpl>::get();
}

void SharedWorkerServiceImpl::AddFilter(SharedWorkerMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  filters_.emplace(filter->render_process_id(), make_scoped_refptr(filter));
}

void SharedWorkerServiceImpl::RemoveFilter(SharedWorkerMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  filters_.erase(filter->render_process_id());
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl()
    : update_worker_dependency_(UpdateWorkerDependency),
      next_pending_instance_id_(0) {
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {}

void SharedWorkerServiceImpl::ResetForTesting() {
  last_worker_depended_renderers_.clear();
  worker_hosts_.clear();
  observers_.Clear();
  update_worker_dependency_ = UpdateWorkerDependency;
  s_try_increment_worker_ref_count_ = TryIncrementWorkerRefCount;
}

scoped_refptr<SharedWorkerMessageFilter> SharedWorkerServiceImpl::GetFilter(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto it = filters_.find(render_process_id);
  if (it != filters_.end())
    return it->second;
  return nullptr;
}

bool SharedWorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  SharedWorkerHost* host = FindSharedWorkerHost(process_id, route_id);
  if (!host || !host->instance())
    return false;
  host->TerminateWorker();
  return true;
}

std::vector<WorkerService::WorkerInfo> SharedWorkerServiceImpl::GetWorkers() {
  std::vector<WorkerService::WorkerInfo> results;
  for (const auto& iter : worker_hosts_) {
    SharedWorkerHost* host = iter.second.get();
    const SharedWorkerInstance* instance = host->instance();
    if (instance) {
      WorkerService::WorkerInfo info;
      info.url = instance->url();
      info.name = instance->name();
      info.route_id = host->worker_route_id();
      info.process_id = host->process_id();
      info.handle = host->worker_render_filter()->PeerHandle();
      results.push_back(info);
    }
  }
  return results;
}

void SharedWorkerServiceImpl::AddObserver(WorkerServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.AddObserver(observer);
}

void SharedWorkerServiceImpl::RemoveObserver(WorkerServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.RemoveObserver(observer);
}

void SharedWorkerServiceImpl::CreateWorker(
    int process_id,
    int frame_id,
    mojom::SharedWorkerInfoPtr info,
    mojom::SharedWorkerClientPtr client,
    const MessagePort& port,
    ResourceContext* resource_context,
    const WorkerStoragePartitionId& partition_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto instance = std::make_unique<SharedWorkerInstance>(
      info->url, base::UTF8ToUTF16(info->name),
      base::UTF8ToUTF16(info->content_security_policy),
      info->content_security_policy_type, info->creation_address_space,
      resource_context, partition_id, info->creation_context_type,
      info->data_saver_enabled);

  auto request =
      std::make_unique<SharedWorkerPendingInstance::SharedWorkerPendingRequest>(
          process_id, frame_id, std::move(client), port,
          instance->creation_context_type());

  if (SharedWorkerPendingInstance* pending = FindPendingInstance(*instance)) {
    pending->AddRequest(std::move(request));
    return;
  }

  auto pending_instance =
      std::make_unique<SharedWorkerPendingInstance>(std::move(instance));
  pending_instance->AddRequest(std::move(request));
  ReserveRenderProcessToCreateWorker(std::move(pending_instance));
}

void SharedWorkerServiceImpl::CountFeature(SharedWorkerMessageFilter* filter,
                                           int worker_route_id,
                                           uint32_t feature) {
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id)) {
    host->CountFeature(static_cast<blink::mojom::WebFeature>(feature));
  }
}

void SharedWorkerServiceImpl::WorkerContextClosed(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  ScopedWorkerDependencyChecker checker(this);
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id))
    host->WorkerContextClosed();
}

void SharedWorkerServiceImpl::WorkerContextDestroyed(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  ScopedWorkerDependencyChecker checker(this);
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id)) {
    host->WorkerContextDestroyed();
  }
  ProcessRouteIdPair key(filter->render_process_id(), worker_route_id);
  worker_hosts_.erase(key);
}

void SharedWorkerServiceImpl::WorkerReadyForInspection(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id))
    host->WorkerReadyForInspection();
}

void SharedWorkerServiceImpl::WorkerScriptLoaded(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id))
    host->WorkerScriptLoaded();
}

void SharedWorkerServiceImpl::WorkerScriptLoadFailed(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  ScopedWorkerDependencyChecker checker(this);
  ProcessRouteIdPair key(filter->render_process_id(), worker_route_id);
  if (!base::ContainsKey(worker_hosts_, key))
    return;
  std::unique_ptr<SharedWorkerHost> host(worker_hosts_[key].release());
  worker_hosts_.erase(key);
  host->WorkerScriptLoadFailed();
}

void SharedWorkerServiceImpl::WorkerConnected(SharedWorkerMessageFilter* filter,
                                              int connection_request_id,
                                              int worker_route_id) {
  if (SharedWorkerHost* host =
          FindSharedWorkerHost(filter->render_process_id(), worker_route_id))
    host->WorkerConnected(connection_request_id);
}

void SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing(
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  std::vector<ProcessRouteIdPair> remove_list;
  for (const auto& it : worker_hosts_) {
    if (it.first.first == filter->render_process_id())
      remove_list.push_back(it.first);
  }
  for (const ProcessRouteIdPair& to_remove : remove_list)
    worker_hosts_.erase(to_remove);

  std::vector<int> remove_pending_instance_list;
  for (const auto& it : pending_instances_) {
    it.second->RemoveRequest(filter->render_process_id());
    if (it.second->requests()->empty())
      remove_pending_instance_list.push_back(it.first);
  }
  for (int to_remove : remove_pending_instance_list)
    pending_instances_.erase(to_remove);
}

void SharedWorkerServiceImpl::NotifyWorkerDestroyed(int worker_process_id,
                                                    int worker_route_id) {
  for (auto& observer : observers_)
    observer.WorkerDestroyed(worker_process_id, worker_route_id);
}

void SharedWorkerServiceImpl::ReserveRenderProcessToCreateWorker(
    std::unique_ptr<SharedWorkerPendingInstance> pending_instance) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!FindPendingInstance(*pending_instance->instance()));
  if (!pending_instance->requests()->size())
    return;

  int worker_process_id = -1;
  int worker_route_id = MSG_ROUTING_NONE;
  bool is_new_worker = true;
  SharedWorkerHost* host = FindSharedWorkerHost(*pending_instance->instance());
  if (host) {
    worker_process_id = host->process_id();
    worker_route_id = host->worker_route_id();
    is_new_worker = false;
  } else {
    // Re-use the process that requested the shared worker.
    SharedWorkerPendingInstance::SharedWorkerPendingRequest* first_request =
        pending_instance->requests()->front().get();
    worker_process_id = first_request->render_process_id;
    scoped_refptr<SharedWorkerMessageFilter> filter =
        GetFilter(worker_process_id);
    // The filter must exist if the pending instance has requests for the
    // corresponding process. See OnSharedWorkerMessageFilterClosing.
    CHECK(filter);
    worker_route_id = filter->GetNextRoutingID();
  }

  const int pending_instance_id = next_pending_instance_id_++;
  SharedWorkerReserver::TryReserve(
      worker_process_id, worker_route_id, is_new_worker,
      *pending_instance->instance(),
      base::Bind(&SharedWorkerServiceImpl::RenderProcessReservedCallback,
                 base::Unretained(this), pending_instance_id),
      base::Bind(&SharedWorkerServiceImpl::RenderProcessReserveFailedCallback,
                 base::Unretained(this), pending_instance_id),
      s_try_increment_worker_ref_count_);
  pending_instances_[pending_instance_id] = std::move(pending_instance);
}

void SharedWorkerServiceImpl::RenderProcessReservedCallback(
    int pending_instance_id,
    int worker_process_id,
    int worker_route_id,
    bool is_new_worker,
    bool pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // To offset the TryIncrementWorkerRefCount called for the reservation,
  // calls DecrementWorkerRefCount after CheckWorkerDependency in
  // ScopeWorkerDependencyChecker's destructor.
  ScopedWorkerDependencyChecker checker(
      this, base::Bind(&DecrementWorkerRefCount, worker_process_id));

  if (!base::ContainsKey(pending_instances_, pending_instance_id))
    return;
  std::unique_ptr<SharedWorkerPendingInstance> pending_instance(
      pending_instances_[pending_instance_id].release());
  pending_instances_.erase(pending_instance_id);

  if (!is_new_worker) {
    SharedWorkerHost* host =
        FindSharedWorkerHost(worker_process_id, worker_route_id);
    if (!host) {
      // Retry reserving a renderer process if the existed Shared Worker was
      // destroyed on IO thread while reserving the renderer process on UI
      // thread.
      ReserveRenderProcessToCreateWorker(std::move(pending_instance));
      return;
    }
    pending_instance->RegisterToSharedWorkerHost(host);
    return;
  }

  scoped_refptr<SharedWorkerMessageFilter> filter =
      GetFilter(worker_process_id);
  if (!filter) {
    pending_instance->RemoveRequest(worker_process_id);
    // Retry reserving a renderer process if the requested renderer process was
    // destroyed on IO thread while reserving the renderer process on UI thread.
    ReserveRenderProcessToCreateWorker(std::move(pending_instance));
    return;
  }

  std::unique_ptr<SharedWorkerHost> host(new SharedWorkerHost(
      pending_instance->release_instance(), filter.get(), worker_route_id));
  const GURL url = host->instance()->url();
  const base::string16 name = host->instance()->name();
  host->Start(pause_on_start);
  pending_instance->RegisterToSharedWorkerHost(host.get());
  ProcessRouteIdPair key(worker_process_id, worker_route_id);
  worker_hosts_[key] = std::move(host);
  for (auto& observer : observers_)
    observer.WorkerCreated(url, name, worker_process_id, worker_route_id);
}

void SharedWorkerServiceImpl::RenderProcessReserveFailedCallback(
    int pending_instance_id,
    int worker_process_id,
    int worker_route_id,
    bool is_new_worker,
    bool pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  worker_hosts_.erase(std::make_pair(worker_process_id, worker_route_id));
  if (!base::ContainsKey(pending_instances_, pending_instance_id))
    return;
  std::unique_ptr<SharedWorkerPendingInstance> pending_instance(
      pending_instances_[pending_instance_id].release());
  pending_instances_.erase(pending_instance_id);
  pending_instance->RemoveRequest(worker_process_id);
  // Retry reserving a renderer process.
  ReserveRenderProcessToCreateWorker(std::move(pending_instance));
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    int render_process_id,
    int worker_route_id) {
  ProcessRouteIdPair key = std::make_pair(render_process_id, worker_route_id);
  if (!base::ContainsKey(worker_hosts_, key))
    return nullptr;
  return worker_hosts_[key].get();
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    const SharedWorkerInstance& instance) {
  for (const auto& iter : worker_hosts_) {
    SharedWorkerHost* host = iter.second.get();
    if (host->IsAvailable() && host->instance()->Matches(instance))
      return host;
  }
  return nullptr;
}

SharedWorkerServiceImpl::SharedWorkerPendingInstance*
SharedWorkerServiceImpl::FindPendingInstance(
    const SharedWorkerInstance& instance) {
  for (const auto& iter : pending_instances_) {
    if (iter.second->instance()->Matches(instance))
      return iter.second.get();
  }
  return nullptr;
}

const std::set<int>
SharedWorkerServiceImpl::GetRenderersWithWorkerDependency() {
  std::set<int> dependent_renderers;
  for (WorkerHostMap::iterator host_iter = worker_hosts_.begin();
       host_iter != worker_hosts_.end();
       ++host_iter) {
    const int process_id = host_iter->first.first;
    if (dependent_renderers.count(process_id))
      continue;
    if (host_iter->second->instance() &&
        host_iter->second->ServesExternalClient())
      dependent_renderers.insert(process_id);
  }
  return dependent_renderers;
}

void SharedWorkerServiceImpl::CheckWorkerDependency() {
  const std::set<int> current_worker_depended_renderers =
      GetRenderersWithWorkerDependency();
  std::vector<int> added_items = base::STLSetDifference<std::vector<int> >(
      current_worker_depended_renderers, last_worker_depended_renderers_);
  std::vector<int> removed_items = base::STLSetDifference<std::vector<int> >(
      last_worker_depended_renderers_, current_worker_depended_renderers);
  if (!added_items.empty() || !removed_items.empty()) {
    last_worker_depended_renderers_ = current_worker_depended_renderers;
    update_worker_dependency_(added_items, removed_items);
  }
}

void SharedWorkerServiceImpl::ChangeUpdateWorkerDependencyFuncForTesting(
    UpdateWorkerDependencyFunc new_func) {
  update_worker_dependency_ = new_func;
}

void SharedWorkerServiceImpl::ChangeTryIncrementWorkerRefCountFuncForTesting(
    bool (*new_func)(int)) {
  s_try_increment_worker_ref_count_ = new_func;
}

}  // namespace content
