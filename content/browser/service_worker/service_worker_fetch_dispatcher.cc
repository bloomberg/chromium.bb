// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    const FetchCallback& callback)
    : version_(version),
      callback_(callback),
      request_(request.Pass()),
      weak_factory_(this) {
}

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
  version_->DispatchFetchEvent(
      *request_.get(),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFinish,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidFinish(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  DCHECK(!callback_.is_null());
  FetchCallback callback = callback_;
  callback.Run(status, fetch_result, response);
}

}  // namespace content
