// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/single_threaded_proxy_resolver.h"

#include "base/thread.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_info.h"

namespace net {

namespace {

class PurgeMemoryTask : public base::RefCountedThreadSafe<PurgeMemoryTask> {
 public:
  explicit PurgeMemoryTask(ProxyResolver* resolver) : resolver_(resolver) {}
  void PurgeMemory() { resolver_->PurgeMemory(); }
 private:
  ProxyResolver* resolver_;
};

}

// SingleThreadedProxyResolver::SetPacScriptTask ------------------------------

// Runs on the worker thread to call ProxyResolver::SetPacScript.
class SingleThreadedProxyResolver::SetPacScriptTask
    : public base::RefCountedThreadSafe<
        SingleThreadedProxyResolver::SetPacScriptTask> {
 public:
  SetPacScriptTask(SingleThreadedProxyResolver* coordinator,
                   const GURL& pac_url,
                   const std::string& pac_bytes,
                   CompletionCallback* callback)
    : coordinator_(coordinator),
      callback_(callback),
      pac_bytes_(pac_bytes),
      pac_url_(pac_url),
      origin_loop_(MessageLoop::current()) {
    DCHECK(callback);
  }

  // Start the SetPacScript request on the worker thread.
  void Start() {
    coordinator_->thread()->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &SetPacScriptTask::DoRequest,
        coordinator_->resolver_.get()));
  }

  void Cancel() {
    // Clear these to inform RequestComplete that it should not try to
    // access them.
    coordinator_ = NULL;
    callback_ = NULL;
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const { return callback_ == NULL; }

 private:
  // Runs on the worker thread.
  void DoRequest(ProxyResolver* resolver) {
    int rv = resolver->expects_pac_bytes() ?
        resolver->SetPacScriptByData(pac_bytes_, NULL) :
        resolver->SetPacScriptByUrl(pac_url_, NULL);

    DCHECK_NE(rv, ERR_IO_PENDING);
    origin_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &SetPacScriptTask::RequestComplete, rv));
  }

  // Runs the completion callback on the origin thread.
  void RequestComplete(int result_code) {
    // The task may have been cancelled after it was started.
    if (!was_cancelled()) {
      CompletionCallback* callback = callback_;
      coordinator_->RemoveOutstandingSetPacScriptTask(this);
      callback->Run(result_code);
    }
  }

  // Must only be used on the "origin" thread.
  SingleThreadedProxyResolver* coordinator_;
  CompletionCallback* callback_;
  std::string pac_bytes_;
  GURL pac_url_;

  // Usable from within DoQuery on the worker thread.
  MessageLoop* origin_loop_;
};

// SingleThreadedProxyResolver::Job -------------------------------------------

class SingleThreadedProxyResolver::Job
    : public base::RefCountedThreadSafe<SingleThreadedProxyResolver::Job> {
 public:
  // |coordinator| -- the SingleThreadedProxyResolver that owns this job.
  // |url|         -- the URL of the query.
  // |results|     -- the structure to fill with proxy resolve results.
  Job(SingleThreadedProxyResolver* coordinator,
      const GURL& url,
      ProxyInfo* results,
      CompletionCallback* callback,
      LoadLog* load_log)
    : coordinator_(coordinator),
      callback_(callback),
      results_(results),
      load_log_(load_log),
      url_(url),
      is_started_(false),
      origin_loop_(MessageLoop::current()) {
    DCHECK(callback);
  }

  // Start the resolve proxy request on the worker thread.
  void Start() {
    is_started_ = true;

    coordinator_->thread()->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Job::DoQuery,
        coordinator_->resolver_.get()));
  }

  bool is_started() const { return is_started_; }

  void Cancel() {
    // Clear these to inform QueryComplete that it should not try to
    // access them.
    coordinator_ = NULL;
    callback_ = NULL;
    results_ = NULL;
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const { return callback_ == NULL; }

 private:
  // Runs on the worker thread.
  void DoQuery(ProxyResolver* resolver) {
    scoped_refptr<LoadLog> worker_log(new LoadLog);
    int rv = resolver->GetProxyForURL(url_, &results_buf_, NULL, NULL,
                                      worker_log);
    DCHECK_NE(rv, ERR_IO_PENDING);

    worker_log->AddRef();  // Balanced in QueryComplete.
    origin_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &Job::QueryComplete, rv, worker_log));
  }

  // Runs the completion callback on the origin thread.
  void QueryComplete(int result_code, LoadLog* worker_log) {
    // Merge the load log that was generated on the worker thread, into the
    // main log.
    if (load_log_)
      load_log_->Append(worker_log);
    worker_log->Release();

    // The Job may have been cancelled after it was started.
    if (!was_cancelled()) {
      if (result_code >= OK) {  // Note: unit-tests use values > 0.
        results_->Use(results_buf_);
      }
      callback_->Run(result_code);

      // We check for cancellation once again, in case the callback deleted
      // the owning ProxyService (whose destructor will in turn cancel us).
      if (!was_cancelled())
        coordinator_->RemoveFrontOfJobsQueueAndStartNext(this);
    }
  }

  // Must only be used on the "origin" thread.
  SingleThreadedProxyResolver* coordinator_;
  CompletionCallback* callback_;
  ProxyInfo* results_;
  scoped_refptr<LoadLog> load_log_;
  GURL url_;
  bool is_started_;

  // Usable from within DoQuery on the worker thread.
  ProxyInfo results_buf_;
  MessageLoop* origin_loop_;
};

// SingleThreadedProxyResolver ------------------------------------------------

SingleThreadedProxyResolver::SingleThreadedProxyResolver(
    ProxyResolver* resolver)
    : ProxyResolver(resolver->expects_pac_bytes()),
      resolver_(resolver) {
}

SingleThreadedProxyResolver::~SingleThreadedProxyResolver() {
  // Cancel the inprogress job (if any), and free the rest.
  for (PendingJobsQueue::iterator it = pending_jobs_.begin();
       it != pending_jobs_.end();
       ++it) {
    (*it)->Cancel();
  }

  if (outstanding_set_pac_script_task_)
    outstanding_set_pac_script_task_->Cancel();

  // Note that |thread_| is destroyed before |resolver_|. This is important
  // since |resolver_| could be running on |thread_|.
}

int SingleThreadedProxyResolver::GetProxyForURL(const GURL& url,
                                                ProxyInfo* results,
                                                CompletionCallback* callback,
                                                RequestHandle* request,
                                                LoadLog* load_log) {
  DCHECK(callback);

  scoped_refptr<Job> job = new Job(this, url, results, callback, load_log);
  pending_jobs_.push_back(job);
  ProcessPendingJobs();  // Jobs can never finish synchronously.

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |request|.
  if (request)
    *request = reinterpret_cast<RequestHandle>(job.get());

  return ERR_IO_PENDING;
}

// There are three states of the request we need to handle:
// (1) Not started (just sitting in the queue).
// (2) Executing Job::DoQuery in the worker thread.
// (3) Waiting for Job::QueryComplete to be run on the origin thread.
void SingleThreadedProxyResolver::CancelRequest(RequestHandle req) {
  DCHECK(req);

  Job* job = reinterpret_cast<Job*>(req);

  bool is_active_job = job->is_started() && !pending_jobs_.empty() &&
      pending_jobs_.front().get() == job;

  job->Cancel();

  if (is_active_job) {
    RemoveFrontOfJobsQueueAndStartNext(job);
    return;
  }

  // Otherwise just delete the job from the queue.
  PendingJobsQueue::iterator it = std::find(
      pending_jobs_.begin(), pending_jobs_.end(), job);
  DCHECK(it != pending_jobs_.end());
  pending_jobs_.erase(it);
}

void SingleThreadedProxyResolver::CancelSetPacScript() {
  DCHECK(outstanding_set_pac_script_task_);
  outstanding_set_pac_script_task_->Cancel();
  outstanding_set_pac_script_task_ = NULL;
}

void SingleThreadedProxyResolver::PurgeMemory() {
  if (thread_.get()) {
    scoped_refptr<PurgeMemoryTask> helper(new PurgeMemoryTask(resolver_.get()));
    thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(helper.get(), &PurgeMemoryTask::PurgeMemory));
  }
}

int SingleThreadedProxyResolver::SetPacScript(
    const GURL& pac_url,
    const std::string& pac_bytes,
    CompletionCallback* callback) {
  EnsureThreadStarted();
  DCHECK(!outstanding_set_pac_script_task_);

  SetPacScriptTask* task = new SetPacScriptTask(
      this, pac_url, pac_bytes, callback);
  outstanding_set_pac_script_task_ = task;
  task->Start();
  return ERR_IO_PENDING;
}

void SingleThreadedProxyResolver::EnsureThreadStarted() {
  if (!thread_.get()) {
    thread_.reset(new base::Thread("pac-thread"));
    thread_->Start();
  }
}

void SingleThreadedProxyResolver::ProcessPendingJobs() {
  if (pending_jobs_.empty())
    return;

  // Get the next job to process (FIFO).
  Job* job = pending_jobs_.front().get();
  if (job->is_started())
    return;

  EnsureThreadStarted();
  job->Start();
}

void SingleThreadedProxyResolver::RemoveFrontOfJobsQueueAndStartNext(
    Job* expected_job) {
  DCHECK_EQ(expected_job, pending_jobs_.front().get());
  pending_jobs_.pop_front();

  // Start next work item.
  ProcessPendingJobs();
}

void SingleThreadedProxyResolver::RemoveOutstandingSetPacScriptTask(
    SetPacScriptTask* task) {
  DCHECK_EQ(outstanding_set_pac_script_task_.get(), task);
  outstanding_set_pac_script_task_ = NULL;
}

}  // namespace net
