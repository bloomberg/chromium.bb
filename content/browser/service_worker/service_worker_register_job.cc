// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

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
      pending_version_(NULL),
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
      &ServiceWorkerRegisterJob::RegisterComplete, weak_factory_.GetWeakPtr()));

  RegistrationCallback start_worker(
      base::Bind(&ServiceWorkerRegisterJob::StartWorkerAndContinue,
                 weak_factory_.GetWeakPtr(),
                 finish_registration));

  UnregistrationCallback register_new(
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
  UnregistrationCallback finish_unregistration(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterComplete,
                 weak_factory_.GetWeakPtr()));

  ServiceWorkerStorage::FindRegistrationCallback unregister(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 finish_unregistration));

  storage_->FindRegistrationForPattern(pattern_, unregister);
}

void ServiceWorkerRegisterJob::StartWorkerAndContinue(
    const StatusCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (registration->active_version()) {
    // We have an active version, so we can complete immediately, even
    // if the service worker isn't running.
    callback.Run(registration, SERVICE_WORKER_OK);
    return;
  }

  pending_version_ = new ServiceWorkerVersion(
      registration, worker_registry_, registration->next_version_id());
  for (std::vector<int>::const_iterator it = pending_process_ids_.begin();
       it != pending_process_ids_.end();
       ++it)
    pending_version_->AddProcessToWorker(*it);

  // The callback to watch "installation" actually fires as soon as
  // the worker is up and running, just before the install event is
  // dispatched. The job will continue to run even though the main
  // callback has executed.
  pending_version_->StartWorker(base::Bind(callback, registration));

  // TODO(alecflett): Don't set the active version until just before
  // the activate event is dispatched.
  registration->set_active_version(pending_version_);
}

void ServiceWorkerRegisterJob::RegisterPatternAndContinue(
    const RegistrationCallback& callback,
    ServiceWorkerStatusCode previous_status) {
  if (previous_status != SERVICE_WORKER_OK) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback,
                   previous_status,
                   scoped_refptr<ServiceWorkerRegistration>()));
    return;
  }

  // TODO: Eventually RegisterInternal will be replaced by an asynchronous
  // operation. Pass its resulting status through 'callback'.
  scoped_refptr<ServiceWorkerRegistration> registration =
      storage_->RegisterInternal(pattern_, script_url_);

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, SERVICE_WORKER_OK,
                                     registration));
}

void ServiceWorkerRegisterJob::UnregisterPatternAndContinue(
    const UnregistrationCallback& callback,
    bool found,
    ServiceWorkerStatusCode previous_status,
    const scoped_refptr<ServiceWorkerRegistration>& previous_registration) {

  // The previous registration may not exist, which is ok.
  if (previous_status == SERVICE_WORKER_OK && found &&
      (script_url_.is_empty() ||
       previous_registration->script_url() != script_url_)) {
    // TODO: Eventually UnregisterInternal will be replaced by an
    // asynchronous operation. Pass its resulting status though
    // 'callback'.
    storage_->UnregisterInternal(pattern_);
    DCHECK(previous_registration->is_shutdown());
  } else {
    // TODO(alecflett): We have an existing registration, we should
    // schedule an update.
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, previous_status));
}

void ServiceWorkerRegisterJob::RunCallbacks(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration);
  }
}

void ServiceWorkerRegisterJob::RegisterComplete(
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode start_status) {
  RunCallbacks(start_status, registration);
  coordinator_->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::UnregisterComplete(
    ServiceWorkerStatusCode status) {
  RunCallbacks(status, NULL);
  coordinator_->FinishJob(pattern_, this);
}

}  // namespace content
