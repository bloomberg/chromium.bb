// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "url/gurl.h"

namespace content {

static void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    ServiceWorkerStorage* storage,
    EmbeddedWorkerRegistry* worker_registry,
    ServiceWorkerJobCoordinator* coordinator,
    const GURL& pattern,
    const GURL& script_url,
    RegistrationType type)
    : storage_(storage),
      worker_registry_(worker_registry),
      coordinator_(coordinator),
      pattern_(pattern),
      script_url_(script_url),
      type_(type),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  // if we've created a pending version, associate source_provider it with
  // that, otherwise queue it up
  callbacks_.push_back(callback);
  DCHECK(process_id != -1);
  if (pending_version_) {
    pending_version_->AddProcessToWorker(process_id);
  } else {
    pending_process_ids_.push_back(process_id);
  }
}

void ServiceWorkerRegisterJob::Start() {
  if (type_ == REGISTER)
    StartRegister();
  else
    StartUnregister();
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJob* job) {
  return job->type_ == type_ &&
         (type_ == ServiceWorkerRegisterJob::UNREGISTER ||
          job->script_url_ == script_url_);
}

void ServiceWorkerRegisterJob::StartRegister() {
  // Set up a chain of callbacks, in reverse order. Each of these
  // callbacks may be called asynchronously by the previous callback.
  StatusCallback finish_registration(base::Bind(
      &ServiceWorkerRegisterJob::Complete, weak_factory_.GetWeakPtr()));

  StatusCallback start_worker(
      base::Bind(&ServiceWorkerRegisterJob::StartWorkerAndContinue,
                 weak_factory_.GetWeakPtr(),
                 finish_registration));

  StatusCallback register_new(
      base::Bind(&ServiceWorkerRegisterJob::RegisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 start_worker));

  ServiceWorkerStorage::FindRegistrationCallback unregister_old(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 register_new));

  storage_->FindRegistrationForPattern(pattern_, unregister_old);
}

void ServiceWorkerRegisterJob::StartUnregister() {
  // Set up a chain of callbacks, in reverse order. Each of these
  // callbacks may be called asynchronously by the previous callback.
  StatusCallback finish_unregistration(
      base::Bind(&ServiceWorkerRegisterJob::Complete,
                 weak_factory_.GetWeakPtr()));

  ServiceWorkerStorage::FindRegistrationCallback unregister(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 finish_unregistration));

  storage_->FindRegistrationForPattern(pattern_, unregister);
}

void ServiceWorkerRegisterJob::StartWorkerAndContinue(
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  DCHECK(registration_);
  if (registration_->active_version()) {
    // We have an active version, so we can complete immediately, even
    // if the service worker isn't running.
    callback.Run(SERVICE_WORKER_OK);
    return;
  }

  pending_version_ = new ServiceWorkerVersion(
      registration_, worker_registry_, registration_->next_version_id());
  for (std::vector<int>::const_iterator it = pending_process_ids_.begin();
       it != pending_process_ids_.end();
       ++it)
    pending_version_->AddProcessToWorker(*it);

  // The callback to watch "installation" actually fires as soon as
  // the worker is up and running, just before the install event is
  // dispatched. The job will continue to run even though the main
  // callback has executed.
  pending_version_->StartWorker(callback);

  // TODO(alecflett): Don't set the active version until just before
  // the activate event is dispatched.
  registration_->set_active_version(pending_version_);
}

void ServiceWorkerRegisterJob::RegisterPatternAndContinue(
    const StatusCallback& callback,
    ServiceWorkerStatusCode previous_status) {
  if (previous_status == SERVICE_WORKER_ERROR_EXISTS) {
    // Registration already exists, call to the next step.
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }

  if (previous_status != SERVICE_WORKER_OK) {
    // Failure case.
    registration_ = NULL;
    Complete(previous_status);
    return;
  }

  DCHECK(!registration_);
  registration_ = new ServiceWorkerRegistration(pattern_, script_url_,
                                                storage_->NewRegistrationId());
  storage_->StoreRegistration(registration_.get(), callback);
}

void ServiceWorkerRegisterJob::UnregisterPatternAndContinue(
    const StatusCallback& callback,
    ServiceWorkerStatusCode previous_status,
    const scoped_refptr<ServiceWorkerRegistration>& previous_registration) {
  if (previous_status == SERVICE_WORKER_OK &&
      (type_ == UNREGISTER ||
       previous_registration->script_url() != script_url_)) {
    // It's unregister, or we have conflicting registration.
    // Unregister it and continue to the next step.
    previous_registration->Shutdown();
    storage_->DeleteRegistration(pattern_, callback);
    return;
  }

  if (previous_status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // The previous registration does not exist, which is ok.
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }

  if (previous_status == SERVICE_WORKER_OK) {
    // We have an existing registration.
    registration_ = previous_registration;
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_EXISTS));
    return;
  }

  RunSoon(base::Bind(callback, previous_status));
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration_);
  }
  coordinator_->FinishJob(pattern_, this);
}

}  // namespace content
