// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include "content/browser/shared_worker/shared_worker_message_filter.h"
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
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
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

}  // namespace content
