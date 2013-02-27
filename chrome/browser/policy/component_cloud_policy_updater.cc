// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/component_cloud_policy_updater.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/component_cloud_policy_store.h"
#include "chrome/browser/policy/proto/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "googleurl/src/gurl.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The maximum size of the serialized policy protobuf.
const size_t kPolicyProtoMaxSize = 16 * 1024;

// The maximum size of the downloaded policy data.
const int64 kPolicyDataMaxSize = 5 * 1024 * 1024;

// Policies for exponential backoff of failed requests. There are 3 policies,
// corresponding to the 3 RetrySchedule enum values below.

// For temporary errors (HTTP 500, RST, etc).
const net::BackoffEntry::Policy kRetrySoonPolicy = {
  // Number of initial errors to ignore before starting to back off.
  0,

  // Initial delay in ms: 60 seconds.
  1000 * 60,

  // Factor by which the waiting time is multiplied.
  2,

  // Fuzzing percentage; this spreads delays randomly between 80% and 100%
  // of the calculated time.
  0.20,

  // Maximum delay in ms: 12 hours.
  1000 * 60 * 60 * 12,

  // When to discard an entry: never.
  -1,

  // |always_use_initial_delay|; false means that the initial delay is
  // applied after the first error, and starts backing off from there.
  false,
};

// For other errors (request failed, server errors).
const net::BackoffEntry::Policy kRetryLaterPolicy = {
  // Number of initial errors to ignore before starting to back off.
  0,

  // Initial delay in ms: 1 hour.
  1000 * 60 * 60,

  // Factor by which the waiting time is multiplied.
  2,

  // Fuzzing percentage; this spreads delays randomly between 80% and 100%
  // of the calculated time.
  0.20,

  // Maximum delay in ms: 12 hours.
  1000 * 60 * 60 * 12,

  // When to discard an entry: never.
  -1,

  // |always_use_initial_delay|; false means that the initial delay is
  // applied after the first error, and starts backing off from there.
  false,
};

// When the data fails validation (maybe because the policy URL and the data
// served at that URL are out of sync). This essentially retries every 12 hours,
// with some random jitter.
const net::BackoffEntry::Policy kRetryMuchLaterPolicy = {
  // Number of initial errors to ignore before starting to back off.
  0,

  // Initial delay in ms: 12 hours.
  1000 * 60 * 60 * 12,

  // Factor by which the waiting time is multiplied.
  2,

  // Fuzzing percentage; this spreads delays randomly between 80% and 100%
  // of the calculated time.
  0.20,

  // Maximum delay in ms: 12 hours.
  1000 * 60 * 60 * 12,

  // When to discard an entry: never.
  -1,

  // |always_use_initial_delay|; false means that the initial delay is
  // applied after the first error, and starts backing off from there.
  false,
};

// Maximum number of retries for requests that aren't likely to get a
// different response (e.g. HTTP 4xx replies).
const int kMaxLimitedRetries = 3;

}  // namespace

// Each FetchJob contains the data about a particular component, and handles
// the downloading of its corresponding data. These objects are owned by the
// updater, and the updater always outlives FetchJobs.
// A FetchJob can be scheduled for a retry later, but its data never changes.
// If the ExternalPolicyData for a particular component changes then a new
// FetchJob is created, and the previous one is discarded.
class ComponentCloudPolicyUpdater::FetchJob
    : public base::SupportsWeakPtr<FetchJob>,
      public net::URLFetcherDelegate {
 public:
  FetchJob(ComponentCloudPolicyUpdater* updater,
           const PolicyNamespace& ns,
           const std::string& serialized_response,
           const em::ExternalPolicyData& data);
  virtual ~FetchJob();

  const PolicyNamespace& policy_namespace() const { return ns_; }

  // Returns true if |other| equals |data_|.
  bool ParamsEquals(const em::ExternalPolicyData& other);

  void StartJob();

  // URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;

 private:
  void OnSucceeded();
  void OnFailed(net::BackoffEntry* backoff_entry);
  void Schedule();

  // Always valid as long as |this| is alive.
  ComponentCloudPolicyUpdater* updater_;

  const PolicyNamespace ns_;
  const std::string serialized_response_;
  const em::ExternalPolicyData data_;

  // If |fetcher_| exists then |this| is the current job, and must call either
  // OnSucceeded or OnFailed.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Some errors should trigger a limited number of retries, even with backoff.
  // This counts the number of such retries, to stop retrying once the limit
  // is reached.
  int limited_retries_count_;

  // Various delays to retry a failed download, depending on the failure reason.
  net::BackoffEntry retry_soon_entry_;
  net::BackoffEntry retry_later_entry_;
  net::BackoffEntry retry_much_later_entry_;

  DISALLOW_COPY_AND_ASSIGN(FetchJob);
};

ComponentCloudPolicyUpdater::FetchJob::FetchJob(
    ComponentCloudPolicyUpdater* updater,
    const PolicyNamespace& ns,
    const std::string& serialized_response,
    const em::ExternalPolicyData& data)
    : updater_(updater),
      ns_(ns),
      serialized_response_(serialized_response),
      data_(data),
      limited_retries_count_(0),
      retry_soon_entry_(&kRetrySoonPolicy),
      retry_later_entry_(&kRetryLaterPolicy),
      retry_much_later_entry_(&kRetryMuchLaterPolicy) {}

ComponentCloudPolicyUpdater::FetchJob::~FetchJob() {
  if (fetcher_) {
    fetcher_.reset();
    // This is the current job; inform the updater that it was cancelled.
    updater_->OnJobFailed(this);
  }
}

bool ComponentCloudPolicyUpdater::FetchJob::ParamsEquals(
    const em::ExternalPolicyData& other) {
  return data_.download_url() == other.download_url() &&
         data_.secure_hash() == other.secure_hash() &&
         data_.download_auth_method() == other.download_auth_method();
}

void ComponentCloudPolicyUpdater::FetchJob::StartJob() {
  fetcher_.reset(net::URLFetcher::Create(
      0, GURL(data_.download_url()), net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(updater_->request_context_);
  fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE |
                         net::LOAD_DISABLE_CACHE |
                         net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_IS_DOWNLOAD |
                         net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SEND_AUTH_DATA);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  fetcher_->Start();
}

void ComponentCloudPolicyUpdater::FetchJob::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == fetcher_.get());

  const net::URLRequestStatus status = source->GetStatus();
  if (status.status() != net::URLRequestStatus::SUCCESS) {
    if (status.error() == net::ERR_CONNECTION_RESET ||
        status.error() == net::ERR_TEMPORARILY_THROTTLED) {
      // The connection was interrupted; try again soon.
      OnFailed(&retry_soon_entry_);
      return;
    } else {
      // Other network error; try again later.
      OnFailed(&retry_later_entry_);
      return;
    }
  } else {
    // Status is success; inspect the HTTP response code.
    if (source->GetResponseCode() >= 500) {
      // Problem at the server; try again soon.
      OnFailed(&retry_soon_entry_);
      return;
    } else if (source->GetResponseCode() >= 400) {
      // Client error; this is unlikely to go away. Retry later, and give up
      // retrying after 3 attempts.
      OnFailed(limited_retries_count_ < kMaxLimitedRetries ? &retry_later_entry_
                                                           : NULL);
      limited_retries_count_++;
      return;
    } else if (source->GetResponseCode() != 200) {
      // Other HTTP failure; try again later.
      OnFailed(&retry_later_entry_);
      return;
    }
  }

  std::string data;
  if (!source->GetResponseAsString(&data) ||
      static_cast<int64>(data.size()) > kPolicyDataMaxSize ||
      !updater_->store_->Store(
          ns_, serialized_response_, data_.secure_hash(), data)) {
    // Failed to retrieve |data|, or it exceeds the size limit, or it failed
    // validation. This may be a temporary error at the download URL.
    OnFailed(&retry_much_later_entry_);
    return;
  }

  OnSucceeded();
}

void ComponentCloudPolicyUpdater::FetchJob::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK(source == fetcher_.get());
  // Reject the data if it exceeds the size limit. The content length is in
  // |total|, and it may be -1 when not known.
  if (current > kPolicyDataMaxSize || total > kPolicyDataMaxSize)
    OnFailed(&retry_much_later_entry_);
}

void ComponentCloudPolicyUpdater::FetchJob::OnSucceeded() {
  fetcher_.reset();
  updater_->OnJobSucceeded(this);
}

void ComponentCloudPolicyUpdater::FetchJob::OnFailed(net::BackoffEntry* entry) {
  fetcher_.reset();

  if (entry) {
    entry->InformOfRequest(false);

    // If new ExternalPolicyData for this component is fetched then this job
    // will be deleted, and the retry task is invalidated. A new job using the
    // new data will be scheduled immediately in that case.
    updater_->task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FetchJob::Schedule, AsWeakPtr()),
        entry->GetTimeUntilRelease());
  }

  updater_->OnJobFailed(this);
}

void ComponentCloudPolicyUpdater::FetchJob::Schedule() {
  updater_->ScheduleJob(this);
}

ComponentCloudPolicyUpdater::ComponentCloudPolicyUpdater(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    ComponentCloudPolicyStore* store)
    : task_runner_(task_runner),
      request_context_(request_context),
      store_(store),
      shutting_down_(false) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
}

ComponentCloudPolicyUpdater::~ComponentCloudPolicyUpdater() {
  DCHECK(CalledOnValidThread());
  shutting_down_ = true;
  STLDeleteValues(&fetch_jobs_);
}

void ComponentCloudPolicyUpdater::UpdateExternalPolicy(
    scoped_ptr<em::PolicyFetchResponse> response) {
  DCHECK(CalledOnValidThread());

  // Keep a serialized copy of |response|, to cache it later.
  // The policy is also rejected if it exceeds the maximum size.
  std::string serialized_response;
  if (!response->SerializeToString(&serialized_response) ||
      serialized_response.size() > kPolicyProtoMaxSize) {
    return;
  }

  // Validate the policy before doing anything else.
  PolicyNamespace ns;
  em::ExternalPolicyData data;
  if (!store_->ValidatePolicy(response.Pass(), &ns, &data)) {
    LOG(ERROR) << "Failed to validate component policy fetched from DMServer";
    return;
  }

  // Maybe the data for this hash has already been downloaded and cached.
  if (data.secure_hash() == store_->GetCachedHash(ns))
    return;

  // TODO(joaodasilva): implement the other two auth methods.
  if (data.download_auth_method() != em::ExternalPolicyData::NONE)
    return;

  // Check for an existing job for this component.
  FetchJob* job = fetch_jobs_[ns];
  if (job) {
    // Check if this data has already been seen.
    if (job->ParamsEquals(data))
      return;

    // The existing job is obsolete, cancel it. If |job| is in the job queue
    // then its WeakPtr will be invalided and skipped in the next StartNextJob.
    // If |job| is the current job then it will immediately call OnJobFailed.
    delete job;
    fetch_jobs_.erase(ns);
  }

  if (data.download_url().empty()) {
    // There is no policy for this component, or the policy has been removed.
    store_->Delete(ns);
  } else {
    // Start a new job with the new or updated data.
    job = new FetchJob(this, ns, serialized_response, data);
    fetch_jobs_[ns] = job;
    ScheduleJob(job);
  }
}

void ComponentCloudPolicyUpdater::ScheduleJob(FetchJob* job) {
  job_queue_.push(job->AsWeakPtr());
  // The job at the front of the queue is always the current job. If |job| is
  // at the front then start it immediately. An invalid job is never at the
  // front; as soon as it becomes invalidated it will call OnJobFailed() and
  // flush the queue.
  if (job == job_queue_.front().get())
    StartNextJob();
}

void ComponentCloudPolicyUpdater::StartNextJob() {
  // Some of the jobs may have been invalidated, and have to be skipped.
  while (!job_queue_.empty() && !job_queue_.front())
    job_queue_.pop();

  // A started job will always call OnJobSucceeded or OnJobFailed.
  if (!job_queue_.empty() && !shutting_down_)
    job_queue_.front()->StartJob();
}

void ComponentCloudPolicyUpdater::OnJobSucceeded(FetchJob* job) {
  DCHECK(fetch_jobs_[job->policy_namespace()] == job);
  DCHECK(!job_queue_.empty() && job_queue_.front() == job);
  fetch_jobs_.erase(job->policy_namespace());
  delete job;
  job_queue_.pop();
  StartNextJob();
}

void ComponentCloudPolicyUpdater::OnJobFailed(FetchJob* job) {
  DCHECK(fetch_jobs_[job->policy_namespace()] == job);
  DCHECK(!job_queue_.empty() && job_queue_.front() == job);
  // The job isn't deleted when it fails because a retry attempt may have been
  // scheduled. It's also kept so that UpdateExternalPolicy() can see the
  // current data, and avoid a new fetch if the data hasn't changed.
  job_queue_.pop();
  StartNextJob();
}

}  // namespace policy
