// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_job_coordinator.h"

#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerJobCoordinator::ServiceWorkerJobCoordinator(
    ServiceWorkerStorage* storage)
    : storage_(storage), weak_factory_(this) {}

ServiceWorkerJobCoordinator::~ServiceWorkerJobCoordinator() {}

void ServiceWorkerJobCoordinator::Register(
    const GURL& pattern,
    const GURL& script_url,
    const ServiceWorkerRegisterJob::RegistrationCallback& callback) {
  scoped_ptr<ServiceWorkerRegisterJob> job(new ServiceWorkerRegisterJob(
      storage_,
      base::Bind(&ServiceWorkerJobCoordinator::RegisterComplete,
                 weak_factory_.GetWeakPtr(),
                 callback)));
  job->StartRegister(pattern, script_url);
  registration_jobs_.push_back(job.release());
}

void ServiceWorkerJobCoordinator::Unregister(
    const GURL& pattern,
    const ServiceWorkerRegisterJob::UnregistrationCallback& callback) {
  scoped_ptr<ServiceWorkerRegisterJob> job(new ServiceWorkerRegisterJob(
      storage_,
      base::Bind(&ServiceWorkerJobCoordinator::UnregisterComplete,
                 weak_factory_.GetWeakPtr(),
                 callback)));
  job->StartUnregister(pattern);
  registration_jobs_.push_back(job.release());
}

void ServiceWorkerJobCoordinator::EraseJob(ServiceWorkerRegisterJob* job) {
  ScopedVector<ServiceWorkerRegisterJob>::iterator job_position =
      registration_jobs_.begin();
  for (; job_position != registration_jobs_.end(); ++job_position) {
    if (*job_position == job) {
      registration_jobs_.erase(job_position);
      return;
    }
  }
  NOTREACHED() << "Deleting non-existent job. ";
}

void ServiceWorkerJobCoordinator::RegisterComplete(
    const ServiceWorkerRegisterJob::RegistrationCallback& callback,
    ServiceWorkerRegisterJob* job,
    ServiceWorkerRegistrationStatus status,
    ServiceWorkerRegistration* registration) {
  callback.Run(status, registration);
  EraseJob(job);
}

void ServiceWorkerJobCoordinator::UnregisterComplete(
    const ServiceWorkerRegisterJob::UnregistrationCallback& callback,
    ServiceWorkerRegisterJob* job,
    ServiceWorkerRegistrationStatus status,
    ServiceWorkerRegistration* previous_registration) {
  callback.Run(status);
  EraseJob(job);
}

}  // namespace content
