// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_job_coordinator.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerJobCoordinator::JobQueue::JobQueue() {}

ServiceWorkerJobCoordinator::JobQueue::~JobQueue() {
  STLDeleteElements(&jobs_);
}

void ServiceWorkerJobCoordinator::JobQueue::Push(
    scoped_ptr<ServiceWorkerRegisterJob> job,
    const ServiceWorkerRegisterJob::RegistrationCallback& callback) {
  if (jobs_.empty()) {
    job->Start();
    jobs_.push_back(job.release());
  } else if (!job->Equals(jobs_.back())) {
    jobs_.push_back(job.release());
  }

  DCHECK(!jobs_.empty());
  jobs_.back()->AddCallback(callback);
}

void ServiceWorkerJobCoordinator::JobQueue::Pop(ServiceWorkerRegisterJob* job) {
  DCHECK(job == jobs_.front());
  jobs_.pop_front();
  delete job;
  if (!jobs_.empty())
    jobs_.front()->Start();
}

ServiceWorkerJobCoordinator::ServiceWorkerJobCoordinator(
    ServiceWorkerStorage* storage)
    : storage_(storage), weak_factory_(this) {}

ServiceWorkerJobCoordinator::~ServiceWorkerJobCoordinator() {}

void ServiceWorkerJobCoordinator::Register(
    const GURL& pattern,
    const GURL& script_url,
    const ServiceWorkerRegisterJob::RegistrationCallback& callback) {
  scoped_ptr<ServiceWorkerRegisterJob> job = make_scoped_ptr(
      new ServiceWorkerRegisterJob(storage_,
                                   this,
                                   pattern,
                                   script_url,
                                   ServiceWorkerRegisterJob::REGISTER));
  jobs_[pattern].Push(job.Pass(), callback);
}

void ServiceWorkerJobCoordinator::Unregister(
    const GURL& pattern,
    const ServiceWorkerRegisterJob::UnregistrationCallback& callback) {

  scoped_ptr<ServiceWorkerRegisterJob> job = make_scoped_ptr(
      new ServiceWorkerRegisterJob(storage_,
                                   this,
                                   pattern,
                                   GURL(),
                                   ServiceWorkerRegisterJob::UNREGISTER));
  jobs_[pattern]
      .Push(job.Pass(),
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
