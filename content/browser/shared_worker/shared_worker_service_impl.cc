// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/worker_host/worker_document_set.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/worker_service_observer.h"

namespace content {

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<SharedWorkerServiceImpl>::get();
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl() {
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {
}

bool SharedWorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  // TODO(horo): implement this.
  return false;
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
  *url_mismatch = false;
  SharedWorkerInstance* existing_instance =
      FindSharedWorkerInstance(
          params.url, params.name, partition, resource_context);
  if (existing_instance) {
    if (params.url != existing_instance->url()) {
      *url_mismatch = true;
      return;
    }
    if (existing_instance->load_failed()) {
      filter->Send(new ViewMsg_WorkerScriptLoadFailed(route_id));
      return;
    }
    existing_instance->AddFilter(filter, route_id);
    existing_instance->worker_document_set()->Add(
        filter, params.document_id, filter->render_process_id(),
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
  instance->AddFilter(filter, route_id);
  instance->worker_document_set()->Add(
      filter, params.document_id, filter->render_process_id(),
      params.render_frame_route_id);

  scoped_ptr<SharedWorkerHost> host(new SharedWorkerHost(instance.release()));
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
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    iter->second->DocumentDetached(filter, document_id);
  }
}

void SharedWorkerServiceImpl::WorkerContextClosed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerContextClosed();
}

void SharedWorkerServiceImpl::WorkerContextDestroyed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
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

SharedWorkerInstance* SharedWorkerServiceImpl::FindSharedWorkerInstance(
    const GURL& url,
    const base::string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerInstance* instance = iter->second->instance();
    if (instance && instance->Matches(url, name, partition, resource_context))
      return instance;
  }
  return NULL;
}

}  // namespace content
