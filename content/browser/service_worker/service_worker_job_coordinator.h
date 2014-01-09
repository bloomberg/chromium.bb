// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistration;

// This class manages all in-flight jobs. Any asynchronous
// operations are run through instances of ServiceWorkerRegisterJob.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  ServiceWorkerJobCoordinator(ServiceWorkerStorage* storage);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& pattern,
                const GURL& script_url,
                const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  void Unregister(
      const GURL& pattern,
      const ServiceWorkerRegisterJob::UnregistrationCallback& callback);

 private:
  friend class ServiceWorkerRegisterJob;

  typedef ScopedVector<ServiceWorkerRegisterJob> RegistrationJobList;

  // Jobs are removed whenever they are finished or canceled.
  void EraseJob(ServiceWorkerRegisterJob* job);

  // Called at ServiceWorkerRegisterJob completion.
  void RegisterComplete(
      const ServiceWorkerRegisterJob::RegistrationCallback& callback,
      ServiceWorkerRegisterJob* job,
      ServiceWorkerRegistrationStatus status,
      ServiceWorkerRegistration* registration);

  // Called at ServiceWorkerRegisterJob completion.
  void UnregisterComplete(
      const ServiceWorkerRegisterJob::UnregistrationCallback& callback,
      ServiceWorkerRegisterJob* job,
      ServiceWorkerRegistrationStatus status,
      ServiceWorkerRegistration* registration);

  // The ServiceWorkerStorage object should always outlive this
  ServiceWorkerStorage* storage_;
  base::WeakPtrFactory<ServiceWorkerJobCoordinator> weak_factory_;
  // A list of currently running jobs. This is a temporary structure until we
  // start managing overlapping registrations explicitly.
  RegistrationJobList registration_jobs_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
