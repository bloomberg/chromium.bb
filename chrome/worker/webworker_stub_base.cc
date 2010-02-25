// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworker_stub_base.h"

#include "base/compiler_specific.h"
#include "chrome/common/child_process.h"
#include "chrome/worker/worker_thread.h"

WebWorkerStubBase::WebWorkerStubBase(int route_id)
    : route_id_(route_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(client_(route_id, this)) {

  // Start processing incoming IPCs for this worker.
  WorkerThread::current()->AddRoute(route_id_, this);
  ChildProcess::current()->AddRefProcess();
}

WebWorkerStubBase::~WebWorkerStubBase() {
  WorkerThread::current()->RemoveRoute(route_id_);
  ChildProcess::current()->ReleaseProcess();
}

void WebWorkerStubBase::Shutdown() {
  // The worker has exited - free ourselves and the client.
  delete this;
}

void WebWorkerStubBase::EnsureWorkerContextTerminates() {
  client_.EnsureWorkerContextTerminates();
}
