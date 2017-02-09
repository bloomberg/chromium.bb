// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/expedited_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "headless/public/util/managed_dispatch_url_request_job.h"
#include "headless/public/util/navigation_request.h"

namespace headless {

ExpeditedDispatcher::ExpeditedDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : io_thread_task_runner_(std::move(io_thread_task_runner)) {}

ExpeditedDispatcher::~ExpeditedDispatcher() {}

void ExpeditedDispatcher::JobCreated(ManagedDispatchURLRequestJob*) {}

void ExpeditedDispatcher::JobKilled(ManagedDispatchURLRequestJob*) {}

void ExpeditedDispatcher::JobFailed(ManagedDispatchURLRequestJob* job,
                                    net::Error error) {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ManagedDispatchURLRequestJob::OnStartError,
                            base::Unretained(job), error));
}

void ExpeditedDispatcher::DataReady(ManagedDispatchURLRequestJob* job) {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ManagedDispatchURLRequestJob::OnHeadersComplete,
                            base::Unretained(job)));
}

void ExpeditedDispatcher::JobDeleted(ManagedDispatchURLRequestJob*) {}

void ExpeditedDispatcher::NavigationRequested(
    std::unique_ptr<NavigationRequest> navigation) {
  // For the ExpeditedDispatcher we don't care when the navigation is done,
  // hence the empty closure.
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NavigationRequest::StartProcessing,
                            std::move(navigation), base::Closure()));
}

}  // namespace headless
