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
  // TODO(horo): implement this.
  std::vector<WorkerService::WorkerInfo> results;
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

  scoped_ptr<SharedWorkerHost> worker(new SharedWorkerHost(instance.release()));
  worker->Init(filter);
  const int worker_route_id = worker->worker_route_id();
  worker_hosts_.push_back(worker.release());

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
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::DocumentDetached(
    unsigned long long document_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::WorkerContextClosed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::WorkerContextDestroyed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::WorkerScriptLoaded(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::WorkerScriptLoadFailed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::WorkerConnected(
    int message_port_id,
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::AllowDatabase(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    unsigned long estimated_size,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::AllowFileSystem(
    int worker_route_id,
    const GURL& url,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::AllowIndexedDB(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing(
    SharedWorkerMessageFilter* filter) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

SharedWorkerInstance* SharedWorkerServiceImpl::FindSharedWorkerInstance(
    const GURL& url,
    const base::string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  for (ScopedVector<SharedWorkerHost>::const_iterator iter =
           worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerInstance* instance = (*iter)->instance();
    if (instance &&
        instance->Matches(url, name, partition, resource_context))
      return instance;
  }
  return NULL;
}

}  // namespace content
