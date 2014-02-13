// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/worker_service_observer.h"

namespace content {

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<SharedWorkerServiceImpl>::get();
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl() {
  // TODO(horo): implement this.
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {
  // TODO(horo): implement this.
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

}  // namespace content
