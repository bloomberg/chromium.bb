// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_thread_impl.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/threading/thread_local.h"
#include "content/child/worker_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/child/worker_thread.h"

namespace content {

namespace {

using WorkerThreadObservers = base::ObserverList<WorkerThread::Observer>;
using ThreadLocalWorkerThreadObservers =
    base::ThreadLocalPointer<WorkerThreadObservers>;

// Stores a WorkerThreadObservers instance per thread.
base::LazyInstance<ThreadLocalWorkerThreadObservers> g_observers_tls =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// WorkerThread:

// static
int WorkerThread::GetCurrentId() {
  if (!g_observers_tls.Get().Get())
    return 0;
  return base::PlatformThread::CurrentId();
}

// static
void WorkerThread::PostTask(int id, const base::Closure& task) {
  WorkerTaskRunner::Instance()->PostTask(id, task);
}

// static
void WorkerThread::AddObserver(Observer* observer) {
  DCHECK(GetCurrentId() > 0);
  WorkerThreadObservers* observers = g_observers_tls.Get().Get();
  DCHECK(observers);
  observers->AddObserver(observer);
}

// static
void WorkerThread::RemoveObserver(Observer* observer) {
  DCHECK(GetCurrentId() > 0);
  WorkerThreadObservers* observers = g_observers_tls.Get().Get();
  DCHECK(observers);
  observers->RemoveObserver(observer);
}

// WorkerThreadImpl:

// static
void WorkerThreadImpl::DidStartCurrentWorkerThread() {
  ThreadLocalWorkerThreadObservers& observers_tls = g_observers_tls.Get();
  DCHECK(!observers_tls.Get());
  observers_tls.Set(new WorkerThreadObservers());
}

// static
void WorkerThreadImpl::WillStopCurrentWorkerThread() {
  ThreadLocalWorkerThreadObservers& observers_tls = g_observers_tls.Get();
  WorkerThreadObservers* observers = observers_tls.Get();
  DCHECK(observers);
  FOR_EACH_OBSERVER(WorkerThread::Observer, *observers,
                    WillStopCurrentWorkerThread());
  delete observers;
  observers_tls.Set(nullptr);
}

}  // namespace content
