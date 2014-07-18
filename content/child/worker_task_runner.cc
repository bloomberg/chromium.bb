// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_task_runner.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"

using blink::WebWorkerRunLoop;

namespace content {

namespace {

class RunLoopRunClosureTask : public WebWorkerRunLoop::Task {
 public:
  RunLoopRunClosureTask(const base::Closure& task) : task_(task) {}
  virtual ~RunLoopRunClosureTask() {}
  virtual void Run() {
    task_.Run();
  }
 private:
  base::Closure task_;
};

class RunClosureTask : public blink::WebThread::Task {
 public:
  RunClosureTask(const base::Closure& task) : task_(task) {}
  virtual ~RunClosureTask() {}
  virtual void run() {
    task_.Run();
  }
 private:
  base::Closure task_;
};

} // namespace

struct WorkerTaskRunner::ThreadLocalState {
  ThreadLocalState(int id, const WebWorkerRunLoop& loop)
      : id_(id), run_loop_(loop), thread_(0) {
  }
  ThreadLocalState(int id, blink::WebThread* thread)
      : id_(id), thread_(thread) {
  }
  int id_;
  WebWorkerRunLoop run_loop_;
  blink::WebThread* thread_;
  ObserverList<WorkerTaskRunner::Observer> stop_observers_;
};

WorkerTaskRunner::WorkerTaskRunner() {
  // Start worker ids at 1, 0 is reserved for the main thread.
  int id = id_sequence_.GetNext();
  DCHECK(!id);
}

bool WorkerTaskRunner::PostTask(
    int id, const base::Closure& closure) {
  DCHECK(id > 0);
  base::AutoLock locker(loop_map_lock_);

  IDToLoopMap::iterator found = loop_map_.find(id);
  if (found != loop_map_.end())
    return found->second.postTask(new RunLoopRunClosureTask(closure));

  IDToThreadMap::iterator thread_found = thread_map_.find(id);
  if (thread_found != thread_map_.end()) {
    thread_found->second->postTask(new RunClosureTask(closure));
    return true;
  }

  return false;
}

int WorkerTaskRunner::PostTaskToAllThreads(const base::Closure& closure) {
  base::AutoLock locker(loop_map_lock_);
  IDToLoopMap::iterator it;
  for (it = loop_map_.begin(); it != loop_map_.end(); ++it)
    it->second.postTask(new RunLoopRunClosureTask(closure));

  IDToThreadMap::iterator iter;
  for (iter = thread_map_.begin(); iter != thread_map_.end(); ++iter)
    iter->second->postTask(new RunClosureTask(closure));

  return static_cast<int>(loop_map_.size() + thread_map_.size());
}

int WorkerTaskRunner::CurrentWorkerId() {
  if (!current_tls_.Get())
    return 0;
  return current_tls_.Get()->id_;
}

WorkerTaskRunner* WorkerTaskRunner::Instance() {
  static base::LazyInstance<WorkerTaskRunner>::Leaky
      worker_task_runner = LAZY_INSTANCE_INITIALIZER;
  return worker_task_runner.Pointer();
}

void WorkerTaskRunner::AddStopObserver(Observer* obs) {
  DCHECK(CurrentWorkerId() > 0);
  current_tls_.Get()->stop_observers_.AddObserver(obs);
}

void WorkerTaskRunner::RemoveStopObserver(Observer* obs) {
  DCHECK(CurrentWorkerId() > 0);
  current_tls_.Get()->stop_observers_.RemoveObserver(obs);
}

WorkerTaskRunner::~WorkerTaskRunner() {
}

void WorkerTaskRunner::OnWorkerRunLoopStarted(const WebWorkerRunLoop& loop) {
  DCHECK(!current_tls_.Get());
  int id = id_sequence_.GetNext();
  current_tls_.Set(new ThreadLocalState(id, loop));

  base::AutoLock locker_(loop_map_lock_);
  loop_map_[id] = loop;
}

void WorkerTaskRunner::OnWorkerRunLoopStopped(const WebWorkerRunLoop& loop) {
  DCHECK(current_tls_.Get());
  FOR_EACH_OBSERVER(Observer, current_tls_.Get()->stop_observers_,
                    OnWorkerRunLoopStopped());
  {
    base::AutoLock locker(loop_map_lock_);
    DCHECK(loop_map_[CurrentWorkerId()] == loop);
    loop_map_.erase(CurrentWorkerId());
  }
  delete current_tls_.Get();
  current_tls_.Set(NULL);
}

void WorkerTaskRunner::OnWorkerThreadStarted(blink::WebThread* thread) {
  DCHECK(!current_tls_.Get());
  int id = id_sequence_.GetNext();
  current_tls_.Set(new ThreadLocalState(id, thread));

  base::AutoLock locker_(loop_map_lock_);
  thread_map_[id] = thread;
}

void WorkerTaskRunner::OnWorkerThreadStopped(blink::WebThread* thread) {
  DCHECK(current_tls_.Get());
  FOR_EACH_OBSERVER(Observer, current_tls_.Get()->stop_observers_,
                    OnWorkerRunLoopStopped());
  {
    base::AutoLock locker(loop_map_lock_);
    DCHECK(thread_map_[CurrentWorkerId()] == thread);
    thread_map_.erase(CurrentWorkerId());
  }
  delete current_tls_.Get();
  current_tls_.Set(NULL);
}

}  // namespace content
