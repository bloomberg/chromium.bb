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
    ServiceWorkerJobCoordinator* coordinator,
    const GURL& pattern,
    const GURL& script_url,
    RegistrationType type)
    : storage_(storage),
      coordinator_(coordinator),
      pattern_(pattern),
      script_url_(script_url),
      type_(type),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::AddCallback(
    const RegistrationCallback& callback) {
  callbacks_.push_back(callback);
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
  RegistrationCallback finish_registration(base::Bind(
      &ServiceWorkerRegisterJob::RegisterComplete, weak_factory_.GetWeakPtr()));

  UnregistrationCallback register_new(
      base::Bind(&ServiceWorkerRegisterJob::RegisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 finish_registration));

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
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  RunCallbacks(status, registration);
  coordinator_->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::UnregisterComplete(
    ServiceWorkerStatusCode status) {
  RunCallbacks(status, NULL);
  coordinator_->FinishJob(pattern_, this);
}

}  // namespace content
