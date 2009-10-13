// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_SINGLE_THREADED_PROXY_RESOLVER_H_
#define NET_PROXY_SINGLE_THREADED_PROXY_RESOLVER_H_

#include <deque>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/proxy/proxy_resolver.h"

namespace base {
class Thread;
}  // namespace base

namespace net {

// ProxyResolver implementation that wraps a synchronous ProxyResolver, and
// runs it on a single worker thread. If multiple requests accumulate, they
// are serviced in FIFO order.
class SingleThreadedProxyResolver : public ProxyResolver {
 public:
  // |resolver| is a synchronous ProxyResolver implementation. It doesn't
  // have to be thread-safe, since it is run on exactly one thread. The
  // constructor takes ownership of |resolver|.
  explicit SingleThreadedProxyResolver(ProxyResolver* resolver);

  virtual ~SingleThreadedProxyResolver();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             LoadLog* load_log);
  virtual void CancelRequest(RequestHandle request);
  virtual void CancelSetPacScript();
  virtual void PurgeMemory();

 protected:
  // The wrapped (synchronous) ProxyResolver.
  ProxyResolver* resolver() { return resolver_.get(); }

 private:
  // Refcounted helper class that bridges between origin thread and worker
  // thread.
  class Job;
  friend class Job;
  class SetPacScriptTask;
  friend class SetPacScriptTask;
  // FIFO queue that contains the in-progress job, and any pending jobs.
  typedef std::deque<scoped_refptr<Job> > PendingJobsQueue;

  base::Thread* thread() { return thread_.get(); }

  // ProxyResolver implementation:
  virtual int SetPacScript(const GURL& pac_url,
                           const std::string& pac_bytes,
                           CompletionCallback* callback);

  // Starts the worker thread if it isn't already running.
  void EnsureThreadStarted();

  // Starts the next job from |pending_jobs_| if possible.
  void ProcessPendingJobs();

  // Removes the front entry of the jobs queue. |expected_job| is our
  // expectation of what the front of the job queue is; it is only used by
  // DCHECK for verification purposes.
  void RemoveFrontOfJobsQueueAndStartNext(Job* expected_job);

  // Clears |outstanding_set_pac_script_task_|.
  // Called when |task| has just finished.
  void RemoveOutstandingSetPacScriptTask(SetPacScriptTask* task);

  // The synchronous resolver implementation.
  scoped_ptr<ProxyResolver> resolver_;

  // The thread where |resolver_| is run on.
  // Note that declaration ordering is important here. |thread_| needs to be
  // destroyed *before* |resolver_|, in case |resolver_| is currently
  // executing on |thread_|.
  scoped_ptr<base::Thread> thread_;

  PendingJobsQueue pending_jobs_;
  scoped_refptr<SetPacScriptTask> outstanding_set_pac_script_task_;
};

}  // namespace net

#endif  // NET_PROXY_SINGLE_THREADED_PROXY_RESOLVER_H_
