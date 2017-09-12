// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_unregister_job.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;

// This class manages all in-flight registration or unregistration jobs.
class CONTENT_EXPORT ServiceWorkerJobCoordinator {
 public:
  explicit ServiceWorkerJobCoordinator(
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerJobCoordinator();

  void Register(const GURL& script_url,
                const ServiceWorkerRegistrationOptions& options,
                ServiceWorkerProviderHost* provider_host,
                const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  void Unregister(
      const GURL& pattern,
      const ServiceWorkerUnregisterJob::UnregistrationCallback& callback);

  void Update(ServiceWorkerRegistration* registration, bool force_bypass_cache);

  void Update(ServiceWorkerRegistration* registration,
              bool force_bypass_cache,
              bool skip_script_comparison,
              ServiceWorkerProviderHost* provider_host,
              const ServiceWorkerRegisterJob::RegistrationCallback& callback);

  // Calls ServiceWorkerRegisterJobBase::Abort() on all jobs and removes them.
  void AbortAll();

  // Removes the job. A job that was not aborted must call FinishJob when it is
  // done.
  void FinishJob(const GURL& pattern, ServiceWorkerRegisterJobBase* job);

 private:
  friend class ServiceWorkerJobTest;

  class JobQueue {
   public:
    JobQueue();
    JobQueue(JobQueue&&);
    ~JobQueue();

    // Adds a job to the queue. If an identical job is already at the end of the
    // queue, no new job is added. Returns the job in the queue, regardless of
    // whether it was newly added.
    ServiceWorkerRegisterJobBase* Push(
        std::unique_ptr<ServiceWorkerRegisterJobBase> job);

    // Returns the first job in the queue.
    ServiceWorkerRegisterJobBase* front() { return jobs_.front().get(); }

    // Starts the first job in the queue.
    void StartOneJob();

    // Removes a job from the queue.
    void Pop(ServiceWorkerRegisterJobBase* job);

    bool empty() { return jobs_.empty(); }

    // Aborts all jobs in the queue and removes them.
    void AbortAll();

    // Marks that the browser is shutting down, so jobs may be destroyed before
    // finishing.
    void ClearForShutdown();

   private:
    base::circular_deque<std::unique_ptr<ServiceWorkerRegisterJobBase>> jobs_;

    DISALLOW_COPY_AND_ASSIGN(JobQueue);
  };

  // Calls JobQueue::Push() and returns the return value.
  ServiceWorkerRegisterJobBase* PushOntoJobQueue(
      const GURL& pattern,
      std::unique_ptr<ServiceWorkerRegisterJobBase> job);

  // The job timeout timer periodically calls MaybeTimeoutJobs(), which aborts
  // the jobs that are taking an excessively long time to complete.
  void StartJobTimeoutTimer();
  void MaybeTimeoutJobs();

  base::TimeDelta GetTickDuration(base::TimeTicks start_time) const;

  // The ServiceWorkerContextCore object should always outlive the
  // job coordinator, the core owns the coordinator.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  std::map<GURL, JobQueue> job_queues_;

  // The job timeout timer interval.
  static constexpr base::TimeDelta kTimeoutTimerDelay =
      base::TimeDelta::FromMinutes(5);

  // The amount of time a running job is given to complete before it is timed
  // out. This value was chosen conservatively based on the timeout for starting
  // a new worker (ServiceWorkerVersion::kStartNewWorkerTimeout = 5 minutes) and
  // the timeout for the 'install' event (ServiceWorkerVersion::kRequestTimeout
  // = 5 minutes).
  static constexpr base::TimeDelta kJobTimeout =
      base::TimeDelta::FromMinutes(30);

  // Starts running whenever there is a non-empty job queue and continues
  // until all jobs are completed or aborted.
  base::RepeatingTimer job_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerJobCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_JOB_COORDINATOR_H_
