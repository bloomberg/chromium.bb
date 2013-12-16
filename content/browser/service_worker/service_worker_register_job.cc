// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    const base::WeakPtr<ServiceWorkerStorage>& storage,
    const RegistrationCompleteCallback& callback)
    : storage_(storage), callback_(callback), weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::StartRegister(const GURL& pattern,
                                             const GURL& script_url) {
  // Set up a chain of callbacks, in reverse order. Each of these
  // callbacks may be called asynchronously by the previous callback.
  ServiceWorkerStorage::RegistrationCallback finish_registration(base::Bind(
      &ServiceWorkerRegisterJob::RegisterComplete, weak_factory_.GetWeakPtr()));

  ServiceWorkerStorage::UnregistrationCallback register_new(
      base::Bind(&ServiceWorkerRegisterJob::RegisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 pattern,
                 script_url,
                 finish_registration));

  ServiceWorkerStorage::FindRegistrationCallback unregister_old(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 pattern,
                 script_url,
                 register_new));

  storage_->FindRegistrationForPattern(pattern, unregister_old);
}

void ServiceWorkerRegisterJob::StartUnregister(const GURL& pattern) {
  // Set up a chain of callbacks, in reverse order. Each of these
  // callbacks may be called asynchronously by the previous callback.
  ServiceWorkerStorage::UnregistrationCallback finish_unregistration(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterComplete,
                 weak_factory_.GetWeakPtr()));

  ServiceWorkerStorage::FindRegistrationCallback unregister(
      base::Bind(&ServiceWorkerRegisterJob::UnregisterPatternAndContinue,
                 weak_factory_.GetWeakPtr(),
                 pattern,
                 GURL(),
                 finish_unregistration));

  storage_->FindRegistrationForPattern(pattern, unregister);
}

void ServiceWorkerRegisterJob::RegisterPatternAndContinue(
    const GURL& pattern,
    const GURL& script_url,
    const ServiceWorkerStorage::RegistrationCallback& callback,
    ServiceWorkerRegistrationStatus previous_status) {
  if (previous_status != REGISTRATION_OK) {
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
      storage_->RegisterInternal(pattern, script_url);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(callback, REGISTRATION_OK, registration));
}

void ServiceWorkerRegisterJob::UnregisterPatternAndContinue(
    const GURL& pattern,
    const GURL& new_script_url,
    const ServiceWorkerStorage::UnregistrationCallback& callback,
    bool found,
    ServiceWorkerRegistrationStatus previous_status,
    const scoped_refptr<ServiceWorkerRegistration>& previous_registration) {

  // The previous registration may not exist, which is ok.
  if (previous_status == REGISTRATION_OK && found &&
      (new_script_url.is_empty() ||
       previous_registration->script_url() != new_script_url)) {
    // TODO: Eventually UnregisterInternal will be replaced by an
    // asynchronous operation. Pass its resulting status though
    // 'callback'.
    storage_->UnregisterInternal(pattern);
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, previous_status));
}

void ServiceWorkerRegisterJob::UnregisterComplete(
    ServiceWorkerRegistrationStatus status) {
  callback_.Run(this, status, NULL);
}

void ServiceWorkerRegisterJob::RegisterComplete(
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  callback_.Run(this, status, registration);
}

}  // namespace content
