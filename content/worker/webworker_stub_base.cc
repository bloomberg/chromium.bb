// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/webworker_stub_base.h"

#include "base/compiler_specific.h"
#include "content/common/child_process.h"
#include "content/worker/worker_thread.h"

WebWorkerStubBase::WebWorkerStubBase(
    int route_id, const WorkerAppCacheInitInfo& appcache_init_info)
    : route_id_(route_id),
      appcache_init_info_(appcache_init_info),
      ALLOW_THIS_IN_INITIALIZER_LIST(client_(route_id, this)) {

  WorkerThread* workerThread = WorkerThread::current();
  DCHECK(workerThread);
  workerThread->AddWorkerStub(this);
  // Start processing incoming IPCs for this worker.
  workerThread->AddRoute(route_id_, this);
  ChildProcess::current()->AddRefProcess();
}

WebWorkerStubBase::~WebWorkerStubBase() {
  WorkerThread* workerThread = WorkerThread::current();
  DCHECK(workerThread);
  workerThread->RemoveWorkerStub(this);
  workerThread->RemoveRoute(route_id_);
  ChildProcess::current()->ReleaseProcess();
}

void WebWorkerStubBase::Shutdown() {
  // The worker has exited - free ourselves and the client.
  delete this;
}

void WebWorkerStubBase::EnsureWorkerContextTerminates() {
  client_.EnsureWorkerContextTerminates();
}
