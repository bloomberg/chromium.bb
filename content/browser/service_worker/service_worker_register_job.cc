// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& pattern,
    const GURL& script_url)
    : context_(context),
      pattern_(pattern),
      script_url_(script_url),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  // If we've created a pending version, associate source_provider it with that,
  // otherwise queue it up.
  callbacks_.push_back(callback);
  DCHECK_NE(-1, process_id);
  if (pending_version_) {
    pending_version_->AddProcessToWorker(process_id);
  } else {
    pending_process_ids_.push_back(process_id);
  }
}

void ServiceWorkerRegisterJob::Start() {
  context_->storage()->FindRegistrationForPattern(
      pattern_,
      base::Bind(
          &ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue,
          weak_factory_.GetWeakPtr()));
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJobBase* job) {
  if (job->GetType() != GetType())
    return false;
  ServiceWorkerRegisterJob* register_job =
      static_cast<ServiceWorkerRegisterJob*>(job);
  return register_job->pattern_ == pattern_ &&
         register_job->script_url_ == script_url_;
}

RegistrationJobType ServiceWorkerRegisterJob::GetType() {
  return REGISTER;
}

// This function corresponds to the steps in Register following
// "Let serviceWorkerRegistration be _GetRegistration(scope)"
// |registration| corresponds to serviceWorkerRegistration.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  // On unexpected error, abort this registration job.
  if (status != SERVICE_WORKER_ERROR_NOT_FOUND && status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "If serviceWorkerRegistration is not null and script is equal to
  // serviceWorkerRegistration.scriptUrl..." resolve with the existing
  // registration and abort.
  if (registration.get() && registration->script_url() == script_url_) {
    registration_ = registration;
    // If there's no active version, go ahead to Update (this isn't in the spec
    // but seems reasonable, and without SoftUpdate implemented we can never
    // Update otherwise).
    if (!registration_->active_version()) {
      UpdateAndContinue(status);
      return;
    }
    RunCallbacks(status, registration_, registration_->active_version());
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "If serviceWorkerRegistration is null..." create a new registration.
  if (!registration.get()) {
    RegisterAndContinue(SERVICE_WORKER_OK);
    return;
  }

  // On script URL mismatch, "set serviceWorkerRegistration.scriptUrl to
  // script." We accomplish this by deleting the existing registration and
  // registering a new one.
  // TODO(falken): Match the spec. We now throw away the active_version_ and
  // pending_version_ of the existing registration, which isn't in the spec.
  context_->storage()->DeleteRegistration(
      pattern_,
      base::Bind(&ServiceWorkerRegisterJob::RegisterAndContinue,
                 weak_factory_.GetWeakPtr()));
}

// Registers a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue(
    ServiceWorkerStatusCode status) {
  DCHECK(!registration_);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  registration_ = new ServiceWorkerRegistration(
      pattern_, script_url_, context_->storage()->NewRegistrationId(),
      context_);
  context_->storage()->StoreRegistration(
      registration_.get(),
      base::Bind(&ServiceWorkerRegisterJob::UpdateAndContinue,
                 weak_factory_.GetWeakPtr()));
}

// This function corresponds to the spec's _Update algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue(
    ServiceWorkerStatusCode status) {
  DCHECK(registration_);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  // TODO: "If serviceWorkerRegistration.pendingWorker is not null..." then
  // terminate the pending worker. It doesn't make sense to implement yet since
  // we always activate the worker if install completed, so there can be no
  // pending worker at this point.
  DCHECK(!registration_->pending_version());

  // TODO: Script fetching and comparing the old and new script belongs here.

  // "Let serviceWorker be a newly-created ServiceWorker object..." and start
  // the worker.
  pending_version_ = new ServiceWorkerVersion(
      registration_, context_->storage()->NewVersionId(), context_);
  for (std::vector<int>::const_iterator it = pending_process_ids_.begin();
       it != pending_process_ids_.end();
       ++it)
    pending_version_->AddProcessToWorker(*it);

  pending_version_->StartWorker(
      base::Bind(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    ServiceWorkerStatusCode status) {
  // "If serviceWorker fails to start up..." then reject the promise with an
  // error and abort.
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "Resolve promise with serviceWorker."
  // Although the spec doesn't set pendingWorker until after resolving the
  // promise, our system's resolving works by passing ServiceWorkerRegistration
  // to the callbacks, so pendingWorker must be set first.
  DCHECK(!registration_->pending_version());
  registration_->set_pending_version(pending_version_);
  RunCallbacks(status, registration_, pending_version_.get());

  // TODO(kinuko): Iterate over all provider hosts and call SetPendingVersion()
  // for documents that are in-scope.

  InstallAndContinue();
}

// This function corresponds to the spec's _Install algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  // "Set serviceWorkerRegistration.pendingWorker._state to installing."
  // "Fire install event on the associated ServiceWorkerGlobalScope object."
  pending_version_->DispatchInstallEvent(
      -1,
      base::Bind(&ServiceWorkerRegisterJob::OnInstallFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    ServiceWorkerStatusCode status) {
  // "If any handler called waitUntil()..." and the resulting promise
  // is rejected, abort.
  if (status != SERVICE_WORKER_OK) {
    registration_->set_pending_version(NULL);
    Complete(status);
    return;
  }

  // TODO: Per spec, only activate if no document is using the registration.
  ActivateAndContinue();
}

// This function corresponds to the spec's _Activate algorithm.
void ServiceWorkerRegisterJob::ActivateAndContinue() {
  // "Set serviceWorkerRegistration.pendingWorker to null."
  registration_->set_pending_version(NULL);

  // TODO: Dispatch the activate event.
  // TODO(michaeln): Persist the newly ACTIVE version.
  pending_version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  DCHECK(!registration_->active_version());
  registration_->set_active_version(pending_version_);
  pending_version_ = NULL;
  Complete(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  // In success case the callbacks must have been dispatched already
  // (so this is no-op), otherwise we must have come here for abort case,
  // so dispatch callbacks with NULL.
  DCHECK(callbacks_.empty() || status != SERVICE_WORKER_OK);
  RunCallbacks(status, NULL, NULL);

  context_->job_coordinator()->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::RunCallbacks(
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration, version);
  }
  callbacks_.clear();
}

}  // namespace content
