// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/external_policy_data_updater.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace policy {

namespace {

// Policies for exponential backoff of failed requests. There are 3 policies for
// different classes of errors.

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

class ExternalPolicyDataUpdater::FetchJob
    : public base::SupportsWeakPtr<FetchJob>,
      public net::URLFetcherDelegate {
 public:
  FetchJob(ExternalPolicyDataUpdater* updater,
           int id,
           const std::string& key,
           const ExternalPolicyDataUpdater::Request& request,
           const ExternalPolicyDataUpdater::FetchSuccessCallback& callback);
  virtual ~FetchJob();

  const std::string& key() const;
  const ExternalPolicyDataUpdater::Request& request() const;

  void Start();

  // URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;

 private:
  void OnFailed(net::BackoffEntry* backoff_entry);
  void Reschedule();

  // Always valid as long as |this| is alive.
  ExternalPolicyDataUpdater* updater_;

  const int id_;
  const std::string key_;
  const ExternalPolicyDataUpdater::Request request_;
  ExternalPolicyDataUpdater::FetchSuccessCallback callback_;

  // If |fetcher_| exists, the job is currently running and must call back to
  // the |updater_|'s OnJobSucceeded() or OnJobFailed() method eventually.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Some errors should trigger a limited number of retries, even with backoff.
  // This counts down the number of such retries to stop retrying once the limit
  // is reached.
  int limited_retries_remaining_;

  // Various delays to retry a failed download, depending on the failure reason.
  net::BackoffEntry retry_soon_entry_;
  net::BackoffEntry retry_later_entry_;
  net::BackoffEntry retry_much_later_entry_;

  DISALLOW_COPY_AND_ASSIGN(FetchJob);
};

ExternalPolicyDataUpdater::Request::Request() {
}

ExternalPolicyDataUpdater::Request::Request(const std::string& url,
                                            const std::string& hash,
                                            int64 max_size)
    : url(url), hash(hash), max_size(max_size) {
}

bool ExternalPolicyDataUpdater::Request::operator==(
    const Request& other) const {
  return url == other.url && hash == other.hash && max_size == other.max_size;
}

ExternalPolicyDataUpdater::FetchJob::FetchJob(
    ExternalPolicyDataUpdater* updater,
    int id,
    const std::string& key,
    const ExternalPolicyDataUpdater::Request& request,
    const ExternalPolicyDataUpdater::FetchSuccessCallback& callback)
    : updater_(updater),
      id_(id),
      key_(key),
      request_(request),
      callback_(callback),
      limited_retries_remaining_(kMaxLimitedRetries),
      retry_soon_entry_(&kRetrySoonPolicy),
      retry_later_entry_(&kRetryLaterPolicy),
      retry_much_later_entry_(&kRetryMuchLaterPolicy) {
}

ExternalPolicyDataUpdater::FetchJob::~FetchJob() {
  if (fetcher_) {
    fetcher_.reset();
    // This job is currently running. Inform the updater that it was canceled.
    updater_->OnJobFailed(this);
  }
}

const std::string& ExternalPolicyDataUpdater::FetchJob::key() const {
  return key_;
}

const ExternalPolicyDataUpdater::Request&
    ExternalPolicyDataUpdater::FetchJob::request() const {
  return request_;
}

void ExternalPolicyDataUpdater::FetchJob::Start() {
  fetcher_.reset(net::URLFetcher::Create(id_, GURL(request_.url),
                                         net::URLFetcher::GET, this));
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

void ExternalPolicyDataUpdater::FetchJob::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);

  const net::URLRequestStatus status = source->GetStatus();

  // The connection was interrupted. Try again soon.
  if (status.error() == net::ERR_CONNECTION_RESET ||
      status.error() == net::ERR_TEMPORARILY_THROTTLED) {
    OnFailed(&retry_soon_entry_);
    return;
  }

  // Another network error occurred. Try again later.
  if (status.status() != net::URLRequestStatus::SUCCESS) {
    OnFailed(&retry_later_entry_);
    return;
  }

  // Problem at the server. Try again soon.
  if (source->GetResponseCode() >= 500) {
    OnFailed(&retry_soon_entry_);
    return;
  }

  // Client error. This is unlikely to go away. Try again later, and give up
  // retrying after 3 attempts.
  if (source->GetResponseCode() >= 400) {
    OnFailed(limited_retries_remaining_ ? &retry_later_entry_ : NULL);
    if (limited_retries_remaining_)
      --limited_retries_remaining_;
    return;
  }

  // Any other type of HTTP failure. Try again later.
  if (source->GetResponseCode() != 200) {
    OnFailed(&retry_later_entry_);
    return;
  }

  std::string data;
  if (!source->GetResponseAsString(&data) ||
      static_cast<int64>(data.size()) > request_.max_size ||
      base::SHA1HashString(data) != request_.hash) {
    // Failed to retrieve |data|, its size exceeds the limit or its hash does
    // not match the expected value. This may be because the data being served
    // is stale. Try again much later.
    OnFailed(&retry_much_later_entry_);
    return;
  }

  // If the callback rejects the data, try again much later.
  if (!callback_.Run(data)) {
    OnFailed(&retry_much_later_entry_);
    return;
  }

  // Signal success.
  fetcher_.reset();
  updater_->OnJobSucceeded(this);
}

void ExternalPolicyDataUpdater::FetchJob::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK_EQ(fetcher_.get(), source);
  // Reject the data if it exceeds the size limit. The content length is in
  // |total|, and it may be -1 when not known.
  if (current > request_.max_size || total > request_.max_size)
    OnFailed(&retry_much_later_entry_);
}

void ExternalPolicyDataUpdater::FetchJob::OnFailed(net::BackoffEntry* entry) {
  fetcher_.reset();

  if (entry) {
    entry->InformOfRequest(false);

    // This function may have been invoked because the job was obsoleted and is
    // in the process of being deleted. If this is the case, the WeakPtr will
    // become invalid and the delayed task will never run.
    updater_->task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FetchJob::Reschedule, AsWeakPtr()),
        entry->GetTimeUntilRelease());
  }

  updater_->OnJobFailed(this);
}

void ExternalPolicyDataUpdater::FetchJob::Reschedule() {
  updater_->ScheduleJob(this);
}

ExternalPolicyDataUpdater::ExternalPolicyDataUpdater(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    size_t max_parallel_fetches)
    : task_runner_(task_runner),
      request_context_(request_context),
      max_parallel_jobs_(max_parallel_fetches),
      running_jobs_(0),
      next_job_id_(0),
      shutting_down_(false) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
}

ExternalPolicyDataUpdater::~ExternalPolicyDataUpdater() {
  DCHECK(CalledOnValidThread());
  shutting_down_ = true;
  STLDeleteValues(&job_map_);
}

void ExternalPolicyDataUpdater::FetchExternalData(
    const std::string key,
    const Request& request,
    const FetchSuccessCallback& callback) {
  DCHECK(CalledOnValidThread());

  // Check whether a job exists for this |key| already.
  FetchJob* job = job_map_[key];
  if (job) {
    // If the current |job| is handling the given |request| already, nothing
    // needs to be done.
    if (job->request() == request)
      return;

    // Otherwise, the current |job| is obsolete. If the |job| is on the queue,
    // its WeakPtr will be invalidated and skipped by StartNextJobs(). If |job|
    // is currently running, it will call OnJobFailed() immediately.
    delete job;
    job_map_.erase(key);
  }

  // Start a new job to handle |request|.
  job = new FetchJob(this, next_job_id_++, key, request, callback);
  job_map_[key] = job;
  ScheduleJob(job);
}

void ExternalPolicyDataUpdater::CancelExternalDataFetch(
    const std::string& key) {
  DCHECK(CalledOnValidThread());

  // If a |job| exists for this |key|, delete it. If the |job| is on the queue,
  // its WeakPtr will be invalidated and skipped by StartNextJobs(). If |job| is
  // currently running, it will call OnJobFailed() immediately.
  std::map<std::string, FetchJob*>::iterator job = job_map_.find(key);
  if (job != job_map_.end()) {
    delete job->second;
    job_map_.erase(job);
  }
}

void ExternalPolicyDataUpdater::StartNextJobs() {
  if (shutting_down_)
    return;

  while (running_jobs_ < max_parallel_jobs_ && !job_queue_.empty()) {
    FetchJob* job = job_queue_.front();
    job_queue_.pop();

    // Some of the jobs may have been invalidated, and have to be skipped.
    if (job) {
      ++running_jobs_;
      // A started job will always call OnJobSucceeded() or OnJobFailed().
      job->Start();
    }
  }
}

void ExternalPolicyDataUpdater::ScheduleJob(FetchJob* job) {
  DCHECK_EQ(job_map_[job->key()], job);

  job_queue_.push(job->AsWeakPtr());

  StartNextJobs();
}

void ExternalPolicyDataUpdater::OnJobSucceeded(FetchJob* job) {
  DCHECK(running_jobs_);
  DCHECK_EQ(job_map_[job->key()], job);

  --running_jobs_;
  job_map_.erase(job->key());
  delete job;

  StartNextJobs();
}

void ExternalPolicyDataUpdater::OnJobFailed(FetchJob* job) {
  DCHECK(running_jobs_);
  DCHECK_EQ(job_map_[job->key()], job);

  --running_jobs_;

  // The job is not deleted when it fails because a retry attempt may have been
  // scheduled.
  StartNextJobs();
}

}  // namespace policy
