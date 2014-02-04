// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

namespace {

void RunSoon(const base::Closure& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(const CallbackArray& callbacks, const Arg& arg) {
  for (typename CallbackArray::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i)
    (*i).Run(arg);
}

}  // namespace

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    EmbeddedWorkerRegistry* worker_registry,
    int64 version_id)
    : version_id_(version_id),
      is_shutdown_(false),
      registration_(registration) {
  if (worker_registry) {
    embedded_worker_ = worker_registry->CreateWorker();
    embedded_worker_->AddObserver(this);
  }
}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
  if (embedded_worker_) {
    embedded_worker_->RemoveObserver(this);
    embedded_worker_.reset();
  }
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  DCHECK(registration_);
  if (status() == RUNNING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (status() == STOPPING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (start_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Start(
        version_id_,
        registration_->script_url());
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  start_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (stop_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Stop();
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  stop_callbacks_.push_back(callback);
}

bool ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request) {
  if (status() != RUNNING)
    return false;
  return embedded_worker_->SendMessage(
      ServiceWorkerMsg_FetchEvent(request)) == SERVICE_WORKER_OK;
}

void ServiceWorkerVersion::AddProcessToWorker(int process_id) {
  DCHECK(!is_shutdown_);
  embedded_worker_->AddProcessReference(process_id);
}

void ServiceWorkerVersion::RemoveProcessToWorker(int process_id) {
  embedded_worker_->ReleaseProcessReference(process_id);
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, status());
  // Fire all start callbacks.
  RunCallbacks(start_callbacks_, SERVICE_WORKER_OK);
  start_callbacks_.clear();
}

void ServiceWorkerVersion::OnStopped() {
  DCHECK_EQ(STOPPED, status());
  // Fire all stop callbacks.
  RunCallbacks(stop_callbacks_, SERVICE_WORKER_OK);
  stop_callbacks_.clear();

  // If there're any callbacks that were waiting start let them know it's
  // failed.
  RunCallbacks(start_callbacks_, SERVICE_WORKER_ERROR_START_WORKER_FAILED);
  start_callbacks_.clear();
}

void ServiceWorkerVersion::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED();
}

}  // namespace content
