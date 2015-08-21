// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/child/worker_thread.h"

#include "content/child/worker_task_runner.h"
#include "content/common/content_export.h"

namespace content {

// static
int WorkerThread::GetCurrentId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

// static
void WorkerThread::PostTask(int id, const base::Closure& task) {
  WorkerTaskRunner::Instance()->PostTask(id, task);
}

}  // namespace content
