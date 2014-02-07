// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_job_coordinator.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerJobCoordinator::JobQueue::JobQueue() {}

ServiceWorkerJobCoordinator::JobQueue::~JobQueue() {
  DCHECK(jobs_.empty()) << "Destroying JobQueue with " << jobs_.size()
                        << " unfinished jobs";
  STLDeleteElements(&jobs_);
}

void ServiceWorkerJobCoordinator::JobQueue::Push(
    scoped_ptr<ServiceWorkerRegisterJob> job,
    int source_process_id,
    const ServiceWorkerRegisterJob::RegistrationCallback& callback) {
  if (jobs_.empty()) {
    job->Start();
    jobs_.push_back(job.release());
  } else if (!job->Equals(jobs_.back())) {
    jobs_.push_back(job.release());
  }
  // Note we are releasing 'job' here.

  DCHECK(!jobs_.empty());
  jobs_.back()->AddCallback(callback, source_process_id);
}

void ServiceWorkerJobCoordinator::JobQueue::Pop(ServiceWorkerRegisterJob* job) {
  DCHECK(job == jobs_.front());
  jobs_.pop_front();
  delete job;
  if (!jobs_.empty())
    jobs_.front()->Start();
}

ServiceWorkerJobCoordinator::ServiceWorkerJobCoordinator(
    ServiceWorkerStorage* storage,
    EmbeddedWorkerRegistry* worker_registry)
    : storage_(storage),
      worker_registry_(worker_registry),
      weak_factory_(this) {}

ServiceWorkerJobCoordinator::~ServiceWorkerJobCoordinator() {
  DCHECK(jobs_.empty()) << "Destroying ServiceWorkerJobCoordinator with "
                        << jobs_.size() << " job queues";
}

void ServiceWorkerJobCoordinator::Register(
    const GURL& pattern,
    const GURL& script_url,
    int source_process_id,
    const ServiceWorkerRegisterJob::RegistrationCallback& callback) {
  scoped_ptr<ServiceWorkerRegisterJob> job = make_scoped_ptr(
      new ServiceWorkerRegisterJob(storage_,
                                   worker_registry_,
                                   this,
                                   pattern,
                                   script_url,
                                   ServiceWorkerRegisterJob::REGISTER));
  jobs_[pattern].Push(job.Pass(), source_process_id, callback);
}

void ServiceWorkerJobCoordinator::Unregister(
    const GURL& pattern,
    int source_process_id,
    const ServiceWorkerRegisterJob::UnregistrationCallback& callback) {

  scoped_ptr<ServiceWorkerRegisterJob> job = make_scoped_ptr(
      new ServiceWorkerRegisterJob(storage_,
                                   worker_registry_,
                                   this,
                                   pattern,
                                   GURL(),
                                   ServiceWorkerRegisterJob::UNREGISTER));
  jobs_[pattern]
      .Push(job.Pass(),
            source_process_id,
            base::Bind(&ServiceWorkerJobCoordinator::UnregisterComplete,
                       weak_factory_.GetWeakPtr(),
                       callback));
}

void ServiceWorkerJobCoordinator::FinishJob(const GURL& pattern,
                                            ServiceWorkerRegisterJob* job) {
  RegistrationJobMap::iterator pending_jobs = jobs_.find(pattern);
  DCHECK(pending_jobs != jobs_.end()) << "Deleting non-existent job.";
  pending_jobs->second.Pop(job);
  if (pending_jobs->second.empty())
    jobs_.erase(pending_jobs);
}

void ServiceWorkerJobCoordinator::UnregisterComplete(
    const ServiceWorkerRegisterJob::UnregistrationCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& previous_registration) {
  callback.Run(status);
}

}  // namespace content
