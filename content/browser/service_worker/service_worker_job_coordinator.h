// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <deque>
#include <map>

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistration;
class EmbeddedWorkerRegistry;

// This class manages all in-flight jobs. Any asynchronous
// operations are run through instances of ServiceWorkerRegisterJob.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(ServiceWorkerStorage* storage,
                                       EmbeddedWorkerRegistry* registry);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& pattern,
                const GURL& script_url,
                int source_process_id,
                const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  void Unregister(
      const GURL& pattern,
      int source_process_id,
      const ServiceWorkerRegisterJob::UnregistrationCallback& callback);

  // Jobs are removed whenever they are finished or canceled.
  void FinishJob(const GURL& pattern, ServiceWorkerRegisterJob* job);

 private:
  friend class ServiceWorkerRegisterJob;

  class JobQueue {
   public:
    JobQueue();
    ~JobQueue();

    void Push(scoped_ptr<ServiceWorkerRegisterJob> job,
              int source_process_id,
              const ServiceWorkerRegisterJob::RegistrationCallback& callback);

    void Pop(ServiceWorkerRegisterJob* job);

    bool empty() { return jobs_.empty(); }

   private:
    std::deque<ServiceWorkerRegisterJob*> jobs_;
  };

  typedef std::map<GURL, JobQueue> RegistrationJobMap;

  // Called at ServiceWorkerRegisterJob completion.
  void UnregisterComplete(
      const ServiceWorkerRegisterJob::UnregistrationCallback& callback,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  // The ServiceWorkerStorage object should always outlive this
  ServiceWorkerStorage* storage_;
  EmbeddedWorkerRegistry* worker_registry_;
  base::WeakPtrFactory<ServiceWorkerJobCoordinator> weak_factory_;

  RegistrationJobMap jobs_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
