// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace policy {

namespace {

// Helper that forwards the result of a fetch job from the thread that the
// ExternalPolicyDataFetcherBackend runs on to the thread that the
// ExternalPolicyDataFetcher which started the job runs on.
void ForwardJobFinished(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const ExternalPolicyDataFetcherBackend::FetchCallback& callback,
    ExternalPolicyDataFetcher::Job* job,
    ExternalPolicyDataFetcher::Result result,
    scoped_ptr<std::string> data) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, job, result, base::Passed(&data)));
}

// Helper that forwards a job cancelation confirmation from the thread that the
// ExternalPolicyDataFetcherBackend runs on to the thread that the
// ExternalPolicyDataFetcher which canceled the job runs on.
void ForwardJobCanceled(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Closure& callback) {
  task_runner->PostTask(FROM_HERE, callback);
}

// Helper invoked when a job cancelation confirmation has been forwarded to the
// thread which canceled the job. The helper itself does nothing. It exists so
// that the |job| can be passed as base::Owned(), allowing it to be deleted on
// the correct thread and after any pending callbacks for the |job| have been
// processed.
void DoNothing(ExternalPolicyDataFetcher::Job* job) {
}

}  // namespace

struct ExternalPolicyDataFetcher::Job {
  Job(const GURL& url,
      int64 max_size,
      const ExternalPolicyDataFetcherBackend::FetchCallback& callback);

  const GURL url;
  const int64 max_size;
  const ExternalPolicyDataFetcherBackend::FetchCallback callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(Job);
};

ExternalPolicyDataFetcher::Job::Job(
    const GURL& url,
    int64 max_size,
    const ExternalPolicyDataFetcherBackend::FetchCallback& callback)
    : url(url),
      max_size(max_size),
      callback(callback) {
}

ExternalPolicyDataFetcher::ExternalPolicyDataFetcher(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const base::WeakPtr<ExternalPolicyDataFetcherBackend>& backend)
    : task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      backend_(backend),
      weak_factory_(this) {
}

ExternalPolicyDataFetcher::~ExternalPolicyDataFetcher() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  for (JobSet::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    CancelJob(*it);
}

ExternalPolicyDataFetcher::Job* ExternalPolicyDataFetcher::StartJob(
    const GURL& url,
    int64 max_size,
    const FetchCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  Job* job = new Job(
      url, max_size,
      base::Bind(&ForwardJobFinished,
                 task_runner_,
                 base::Bind(&ExternalPolicyDataFetcher::OnJobFinished,
                            weak_factory_.GetWeakPtr(),
                            callback)));
  jobs_.insert(job);
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalPolicyDataFetcherBackend::StartJob, backend_, job));
  return job;
}

void ExternalPolicyDataFetcher::CancelJob(Job* job) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(jobs_.find(job) != jobs_.end());
  jobs_.erase(job);
  // Post a task that will cancel the |job| in the |backend_|. The |job| is
  // removed from |jobs_| immediately to indicate that it has been canceled but
  // is not actually deleted until the cancelation has reached the |backend_|
  // and a confirmation has been posted back. This ensures that no new job can
  // be allocated at the same address while an OnJobFinished() callback may
  // still be pending for the canceled |job|.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalPolicyDataFetcherBackend::CancelJob,
                 backend_,
                 job,
                 base::Bind(&ForwardJobCanceled,
                            task_runner_,
                            base::Bind(&DoNothing, base::Owned(job)))));
}

void ExternalPolicyDataFetcher::OnJobFinished(const FetchCallback& callback,
                                              Job* job,
                                              Result result,
                                              scoped_ptr<std::string> data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  JobSet::iterator it = jobs_.find(job);
  if (it == jobs_.end()) {
    // The |job| has been canceled and removed from |jobs_| already. This can
    // happen because the |backend_| runs on a different thread and a |job| may
    // finish before the cancellation has reached that thread.
    return;
  }
  callback.Run(result, data.Pass());
  jobs_.erase(it);
  delete job;
}

ExternalPolicyDataFetcherBackend::ExternalPolicyDataFetcherBackend(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context)
    : io_task_runner_(io_task_runner),
      request_context_(request_context),
      last_fetch_id_(-1),
      weak_factory_(this) {
}

ExternalPolicyDataFetcherBackend::~ExternalPolicyDataFetcherBackend() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  STLDeleteContainerPairFirstPointers(job_map_.begin(), job_map_.end());
}

scoped_ptr<ExternalPolicyDataFetcher>
    ExternalPolicyDataFetcherBackend::CreateFrontend(
        scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return make_scoped_ptr(new ExternalPolicyDataFetcher(
      task_runner, io_task_runner_, weak_factory_.GetWeakPtr()));
}

void ExternalPolicyDataFetcherBackend::StartJob(
    ExternalPolicyDataFetcher::Job* job) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      ++last_fetch_id_, job->url, net::URLFetcher::GET, this);
  fetcher->SetRequestContext(request_context_.get());
  fetcher->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_IS_DOWNLOAD |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA);
  fetcher->SetAutomaticallyRetryOnNetworkChanges(3);
  fetcher->Start();
  job_map_[fetcher] = job;
}

void ExternalPolicyDataFetcherBackend::CancelJob(
    ExternalPolicyDataFetcher::Job* job,
    const base::Closure& callback) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  for (JobMap::iterator it = job_map_.begin(); it != job_map_.end(); ) {
    if (it->second == job) {
      delete it->first;
      job_map_.erase(it++);
    } else {
      ++it;
    }
  }
  callback.Run();
}

void ExternalPolicyDataFetcherBackend::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  JobMap::iterator it = job_map_.find(const_cast<net::URLFetcher*>(source));
  if (it == job_map_.end()) {
    NOTREACHED();
    return;
  }

  ExternalPolicyDataFetcher::Result result = ExternalPolicyDataFetcher::SUCCESS;
  scoped_ptr<std::string> data;

  const net::URLRequestStatus status = it->first->GetStatus();
  if (status.error() == net::ERR_CONNECTION_RESET ||
      status.error() == net::ERR_TEMPORARILY_THROTTLED) {
    // The connection was interrupted.
    result = ExternalPolicyDataFetcher::CONNECTION_INTERRUPTED;
  } else if (status.status() != net::URLRequestStatus::SUCCESS) {
    // Another network error occurred.
    result = ExternalPolicyDataFetcher::NETWORK_ERROR;
  } else if (source->GetResponseCode() >= 500) {
    // Problem at the server.
    result = ExternalPolicyDataFetcher::SERVER_ERROR;
  } else if (source->GetResponseCode() >= 400) {
    // Client error.
    result = ExternalPolicyDataFetcher::CLIENT_ERROR;
  } else if (source->GetResponseCode() != 200) {
    // Any other type of HTTP failure.
    result = ExternalPolicyDataFetcher::HTTP_ERROR;
  } else {
    data.reset(new std::string);
    source->GetResponseAsString(data.get());
    if (static_cast<int64>(data->size()) > it->second->max_size) {
      // Received |data| exceeds maximum allowed size.
      data.reset();
      result = ExternalPolicyDataFetcher::MAX_SIZE_EXCEEDED;
    }
  }

  ExternalPolicyDataFetcher::Job* job = it->second;
  delete it->first;
  job_map_.erase(it);
  job->callback.Run(job, result, data.Pass());
}

void ExternalPolicyDataFetcherBackend::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  JobMap::iterator it = job_map_.find(const_cast<net::URLFetcher*>(source));
  DCHECK(it != job_map_.end());
  if (it == job_map_.end())
    return;

  // Reject the data if it exceeds the size limit. The content length is in
  // |total|, and it may be -1 when not known.
  if (current > it->second->max_size || total > it->second->max_size) {
    ExternalPolicyDataFetcher::Job* job = it->second;
    delete it->first;
    job_map_.erase(it);
    job->callback.Run(job,
                      ExternalPolicyDataFetcher::MAX_SIZE_EXCEEDED,
                      scoped_ptr<std::string>());
  }
}

}  // namespace policy
