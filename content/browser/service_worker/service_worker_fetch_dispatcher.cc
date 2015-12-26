// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback)
    : version_(version),
      prepare_callback_(prepare_callback),
      fetch_callback_(fetch_callback),
      request_(std::move(request)),
      weak_factory_(this) {}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    version_->RegisterStatusChangeCallback(
        base::Bind(&ServiceWorkerFetchDispatcher::DidWaitActivation,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidWaitActivation() {
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::INSTALLED, version_->status());
    DidFailActivation();
    return;
  }
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidFailActivation() {
  // The previous activation seems to have failed, abort the step
  // with activate error. (The error should be separately reported
  // to the associated documents and association must be dropped
  // at this point)
  DidFinish(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED,
            SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
            ServiceWorkerResponse());
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker",
      "ServiceWorkerFetchDispatcher::DispatchFetchEvent",
      request_.get());
  version_->DispatchFetchEvent(
      *request_.get(),
      base::Bind(&ServiceWorkerFetchDispatcher::DidPrepare,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFinish,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidPrepare() {
  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();
}

void ServiceWorkerFetchDispatcher::DidFinish(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  TRACE_EVENT_ASYNC_END0(
      "ServiceWorker",
      "ServiceWorkerFetchDispatcher::DispatchFetchEvent",
      request_.get());
  DCHECK(!fetch_callback_.is_null());
  FetchCallback fetch_callback = fetch_callback_;
  fetch_callback.Run(status, fetch_result, response, version_);
}

}  // namespace content
