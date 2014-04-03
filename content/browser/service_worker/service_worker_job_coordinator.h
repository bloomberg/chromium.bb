// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <deque>
#include <map>

#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_unregister_job.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;

// This class manages all in-flight registration or unregistration jobs.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& pattern,
                const GURL& script_url,
                int source_process_id,
                const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  void Unregister(
      const GURL& pattern,
      int source_process_id,
      const ServiceWorkerUnregisterJob::UnregistrationCallback& callback);

  // Jobs are removed whenever they are finished or canceled.
  void FinishJob(const GURL& pattern, ServiceWorkerRegisterJobBase* job);

 private:
  class JobQueue {
   public:
    JobQueue();
    ~JobQueue();

    // Adds a job to the queue. If an identical job is already in the queue, no
    // new job is added. Returns the job in the queue, regardless of whether it
    // was newly added.
    ServiceWorkerRegisterJobBase* Push(
        scoped_ptr<ServiceWorkerRegisterJobBase> job);

    // Removes a job from the queue.
    void Pop(ServiceWorkerRegisterJobBase* job);

    bool empty() { return jobs_.empty(); }

   private:
    std::deque<ServiceWorkerRegisterJobBase*> jobs_;
  };

  typedef std::map<GURL, JobQueue> RegistrationJobMap;

  // The ServiceWorkerContextCore object should always outlive the
  // job coordinator, the core owns the coordinator.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  RegistrationJobMap jobs_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
