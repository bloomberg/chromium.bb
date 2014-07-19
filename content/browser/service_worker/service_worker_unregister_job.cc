// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_unregister_job.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerUnregisterJob::ServiceWorkerUnregisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& pattern)
    : context_(context),
      pattern_(pattern),
      weak_factory_(this) {}

ServiceWorkerUnregisterJob::~ServiceWorkerUnregisterJob() {}

void ServiceWorkerUnregisterJob::AddCallback(
    const UnregistrationCallback& callback) {
  callbacks_.push_back(callback);
}

void ServiceWorkerUnregisterJob::Start() {
  context_->storage()->FindRegistrationForPattern(
      pattern_,
      base::Bind(&ServiceWorkerUnregisterJob::OnRegistrationFound,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerUnregisterJob::Abort() {
  CompleteInternal(SERVICE_WORKER_ERROR_ABORT);
}

bool ServiceWorkerUnregisterJob::Equals(ServiceWorkerRegisterJobBase* job) {
  if (job->GetType() != GetType())
    return false;
  return static_cast<ServiceWorkerUnregisterJob*>(job)->pattern_ == pattern_;
}

RegistrationJobType ServiceWorkerUnregisterJob::GetType() {
  return UNREGISTRATION_JOB;
}

void ServiceWorkerUnregisterJob::OnRegistrationFound(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    DCHECK(!registration);
    Complete(SERVICE_WORKER_OK);
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  DCHECK(registration);
  DeleteRegistration(registration);
}

void ServiceWorkerUnregisterJob::DeleteRegistration(
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  // TODO: Also doom installing version.
  if (registration->waiting_version())
    registration->waiting_version()->Doom();
  if (registration->active_version())
    registration->active_version()->Doom();

  context_->storage()->DeleteRegistration(
      registration->id(),
      registration->script_url().GetOrigin(),
      base::Bind(&ServiceWorkerUnregisterJob::Complete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerUnregisterJob::Complete(ServiceWorkerStatusCode status) {
  CompleteInternal(status);
  context_->job_coordinator()->FinishJob(pattern_, this);
}

void ServiceWorkerUnregisterJob::CompleteInternal(
    ServiceWorkerStatusCode status) {
  for (std::vector<UnregistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status);
  }
}

}  // namespace content
