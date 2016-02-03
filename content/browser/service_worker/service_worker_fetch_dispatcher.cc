// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"

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
  DidWaitActivation();
}

void ServiceWorkerFetchDispatcher::DidWaitActivation() {
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::INSTALLED, version_->status());
    DidFailActivation();
    return;
  }
  version_->RunAfterStartWorker(
      base::Bind(&ServiceWorkerFetchDispatcher::DispatchFetchEvent,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFail,
                 weak_factory_.GetWeakPtr()));
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
  DCHECK_EQ(ServiceWorkerVersion::RUNNING, version_->running_status())
      << "Worker stopped too soon after it was started.";

  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();

  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::FETCH,
                             base::Bind(&ServiceWorkerFetchDispatcher::DidFail,
                                        weak_factory_.GetWeakPtr()));
  version_->DispatchEvent<ServiceWorkerHostMsg_FetchEventFinished>(
      request_id, ServiceWorkerMsg_FetchEvent(request_id, *request_.get()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFinish,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidPrepare() {
  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();
}

void ServiceWorkerFetchDispatcher::DidFail(ServiceWorkerStatusCode status) {
  DCHECK(!fetch_callback_.is_null());
  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(status, SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                     ServiceWorkerResponse(), version);
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  const bool handled =
      (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  if (!version_->FinishRequest(request_id, handled))
    NOTREACHED() << "Should only receive one reply per event";

  DCHECK(!fetch_callback_.is_null());
  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(SERVICE_WORKER_OK, fetch_result, response, version);
}

}  // namespace content
