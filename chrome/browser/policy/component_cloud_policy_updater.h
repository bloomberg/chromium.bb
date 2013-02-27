// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_COMPONENT_CLOUD_POLICY_UPDATER_H_
#define CHROME_BROWSER_POLICY_COMPONENT_CLOUD_POLICY_UPDATER_H_

#include <map>
#include <queue>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/policy/policy_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ComponentCloudPolicyStore;

// This class downloads external policy data, given PolicyFetchResponses.
// It validates the PolicyFetchResponse and its corresponding data, and caches
// them in a ComponentCloudPolicyStore. It also enforces size limits on what's
// cached.
// It retries to download the policy data periodically when a download fails.
class ComponentCloudPolicyUpdater : public base::NonThreadSafe {
 public:
  // |task_runner| must support file I/O, and is used to post delayed retry
  // tasks.
  // |request_context| will be used for the download fetchers.
  // |store| must outlive the updater, and is where the downloaded data will
  // be cached.
  ComponentCloudPolicyUpdater(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      ComponentCloudPolicyStore* store);
  ~ComponentCloudPolicyUpdater();

  // |response| is the latest policy information fetched for some component.
  // This method schedules the download of the policy data, if |response| is
  // validated. If the downloaded data also passes validation then that data
  // will be passed to the |store_|.
  void UpdateExternalPolicy(
      scoped_ptr<enterprise_management::PolicyFetchResponse> response);

 private:
  class FetchJob;
  typedef std::map<PolicyNamespace, FetchJob*> FetchJobMap;

  // Starts |job| if the job queue is empty, otherwise schedules it.
  void ScheduleJob(FetchJob* job);

  // Appends |job| to the |job_queue_|, and starts it immediately if it's the
  // only scheduled job.
  void StartNextJob();

  // Callback for jobs that succeeded.
  void OnJobSucceeded(FetchJob* job);

  // Callback for jobs that failed.
  void OnJobFailed(FetchJob* job);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  ComponentCloudPolicyStore* store_;

  // Map of jobs that have been scheduled and haven't succeeded yet. Failing
  // jobs stay in |fetch_jobs_|, and may have retries scheduled in the future.
  // This map owns all the currently existing jobs.
  FetchJobMap fetch_jobs_;

  // Queue of jobs to start as soon as possible. Each job starts once the
  // previous job completes, to avoid starting too many downloads in parallel
  // at once. The job at the front of the queue is the currently executing
  // job, and will call OnJobSucceeded or OnJobFailed.
  std::queue<base::WeakPtr<FetchJob> > job_queue_;

  // True once the destructor enters. Prevents jobs from being scheduled during
  // shutdown.
  bool shutting_down_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_COMPONENT_CLOUD_POLICY_UPDATER_H_
