// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <vector>

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/worker_host/worker_document_set.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/worker_service_observer.h"

namespace content {
namespace {

class ScopedWorkerDependencyChecker {
 public:
  explicit ScopedWorkerDependencyChecker(SharedWorkerServiceImpl* service)
      : service_(service) {}
  ~ScopedWorkerDependencyChecker() { service_->CheckWorkerDependency(); }

 private:
  SharedWorkerServiceImpl* service_;
  DISALLOW_COPY_AND_ASSIGN(ScopedWorkerDependencyChecker);
};

void UpdateWorkerDependencyOnUI(const std::vector<int>& added_ids,
                                const std::vector<int>& removed_ids) {
  for (size_t i = 0; i < added_ids.size(); ++i) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(
            RenderProcessHost::FromID(added_ids[i]));
    if (!render_process_host_impl)
      continue;
    render_process_host_impl->IncrementWorkerRefCount();
  }
  for (size_t i = 0; i < removed_ids.size(); ++i) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(
            RenderProcessHost::FromID(removed_ids[i]));
    if (!render_process_host_impl)
      continue;
    render_process_host_impl->DecrementWorkerRefCount();
  }
}

void UpdateWorkerDependency(const std::vector<int>& added_ids,
                            const std::vector<int>& removed_ids) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UpdateWorkerDependencyOnUI, added_ids, removed_ids));
}

}  // namespace

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<SharedWorkerServiceImpl>::get();
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl()
    : update_worker_dependency_(UpdateWorkerDependency) {}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {}

void SharedWorkerServiceImpl::ResetForTesting() {
  last_worker_depended_renderers_.clear();
  worker_hosts_.clear();
  observers_.Clear();
  update_worker_dependency_ = UpdateWorkerDependency;
}

bool SharedWorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  SharedWorkerHost* host =
      worker_hosts_.get(std::make_pair(process_id, route_id));
  if (!host || !host->instance())
    return false;
  host->TerminateWorker();
  return true;
}

std::vector<WorkerService::WorkerInfo> SharedWorkerServiceImpl::GetWorkers() {
  std::vector<WorkerService::WorkerInfo> results;
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerHost* host = iter->second;
    const SharedWorkerInstance* instance = host->instance();
    if (instance) {
      WorkerService::WorkerInfo info;
      info.url = instance->url();
      info.name = instance->name();
      info.route_id = host->worker_route_id();
      info.process_id = host->process_id();
      info.handle = host->container_render_filter()->PeerHandle();
      results.push_back(info);
    }
  }
  return results;
}

void SharedWorkerServiceImpl::AddObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void SharedWorkerServiceImpl::RemoveObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.RemoveObserver(observer);
}

void SharedWorkerServiceImpl::CreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    SharedWorkerMessageFilter* filter,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    bool* url_mismatch) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ScopedWorkerDependencyChecker checker(this);
  *url_mismatch = false;
  SharedWorkerHost* existing_host = FindSharedWorkerHost(
      params.url, params.name, partition, resource_context);
  if (existing_host) {
    if (params.url != existing_host->instance()->url()) {
      *url_mismatch = true;
      return;
    }
    if (existing_host->load_failed()) {
      filter->Send(new ViewMsg_WorkerScriptLoadFailed(route_id));
      return;
    }
    existing_host->AddFilter(filter, route_id);
    existing_host->worker_document_set()->Add(filter,
                                              params.document_id,
                                              filter->render_process_id(),
                                              params.render_frame_route_id);
    filter->Send(new ViewMsg_WorkerCreated(route_id));
    return;
  }

  scoped_ptr<SharedWorkerInstance> instance(new SharedWorkerInstance(
      params.url,
      params.name,
      params.content_security_policy,
      params.security_policy_type,
      resource_context,
      partition));
  scoped_ptr<SharedWorkerHost> host(new SharedWorkerHost(instance.release()));
  host->AddFilter(filter, route_id);
  host->worker_document_set()->Add(filter,
                                   params.document_id,
                                   filter->render_process_id(),
                                   params.render_frame_route_id);

  host->Init(filter);
  const int worker_route_id = host->worker_route_id();
  worker_hosts_.set(std::make_pair(filter->render_process_id(),
                                   worker_route_id),
                    host.Pass());

  FOR_EACH_OBSERVER(
      WorkerServiceObserver, observers_,
      WorkerCreated(params.url,
                    params.name,
                    filter->render_process_id(),
                    worker_route_id));
}

void SharedWorkerServiceImpl::ForwardToWorker(
    const IPC::Message& message,
    SharedWorkerMessageFilter* filter) {
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    if (iter->second->FilterMessage(message, filter))
      return;
  }
}

void SharedWorkerServiceImpl::DocumentDetached(
    unsigned long long document_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    iter->second->DocumentDetached(filter, document_id);
  }
}

void SharedWorkerServiceImpl::WorkerContextClosed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerContextClosed();
}

void SharedWorkerServiceImpl::WorkerContextDestroyed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  scoped_ptr<SharedWorkerHost> host =
      worker_hosts_.take_and_erase(std::make_pair(filter->render_process_id(),
                                                  worker_route_id));
  if (!host)
    return;
  host->WorkerContextDestroyed();
}

void SharedWorkerServiceImpl::WorkerScriptLoaded(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerScriptLoaded();
}

void SharedWorkerServiceImpl::WorkerScriptLoadFailed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  scoped_ptr<SharedWorkerHost> host =
      worker_hosts_.take_and_erase(std::make_pair(filter->render_process_id(),
                                                  worker_route_id));
  if (!host)
    return;
  host->WorkerScriptLoadFailed();
}

void SharedWorkerServiceImpl::WorkerConnected(
    int message_port_id,
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerConnected(message_port_id);
}

void SharedWorkerServiceImpl::AllowDatabase(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    unsigned long estimated_size,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->AllowDatabase(url, name, display_name, estimated_size, result);
}

void SharedWorkerServiceImpl::AllowFileSystem(
    int worker_route_id,
    const GURL& url,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->AllowFileSystem(url, result);
}

void SharedWorkerServiceImpl::AllowIndexedDB(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->AllowIndexedDB(url, name, result);
}

void SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing(
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  std::vector<ProcessRouteIdPair> remove_list;
  for (WorkerHostMap::iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    iter->second->FilterShutdown(filter);
    if (iter->first.first == filter->render_process_id())
      remove_list.push_back(iter->first);
  }
  for (size_t i = 0; i < remove_list.size(); ++i)
    worker_hosts_.erase(remove_list[i]);
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  return worker_hosts_.get(std::make_pair(filter->render_process_id(),
                                          worker_route_id));
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    const GURL& url,
    const base::string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerInstance* instance = iter->second->instance();
    if (instance && !iter->second->closed() &&
        instance->Matches(url, name, partition, resource_context))
      return iter->second;
  }
  return NULL;
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
        host_iter->second->worker_document_set()->ContainsExternalRenderer(
            process_id)) {
      dependent_renderers.insert(process_id);
    }
  }
  return dependent_renderers;
}

void SharedWorkerServiceImpl::CheckWorkerDependency() {
  const std::set<int> current_worker_depended_renderers =
      GetRenderersWithWorkerDependency();
  std::vector<int> added_items;
  std::vector<int> removed_items;
  std::set_difference(current_worker_depended_renderers.begin(),
                      current_worker_depended_renderers.end(),
                      last_worker_depended_renderers_.begin(),
                      last_worker_depended_renderers_.end(),
                      std::back_inserter(added_items));
  std::set_difference(last_worker_depended_renderers_.begin(),
                      last_worker_depended_renderers_.end(),
                      current_worker_depended_renderers.begin(),
                      current_worker_depended_renderers.end(),
                      std::back_inserter(removed_items));
  if (!added_items.empty() || !removed_items.empty()) {
    last_worker_depended_renderers_ = current_worker_depended_renderers;
    update_worker_dependency_(added_items, removed_items);
  }
}

void SharedWorkerServiceImpl::ChangeUpdateWorkerDependencyFuncForTesting(
    UpdateWorkerDependencyFunc new_func) {
  update_worker_dependency_ = new_func;
}

}  // namespace content
