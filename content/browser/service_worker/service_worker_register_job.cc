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
      phase_(INITIAL),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  // If we've created a pending version, associate source_provider it with that,
  // otherwise queue it up.
  callbacks_.push_back(callback);
  DCHECK_NE(-1, process_id);
  if (phase_ >= UPDATE && pending_version()) {
    pending_version()->AddProcessToWorker(process_id);
  } else {
    pending_process_ids_.push_back(process_id);
  }
}

void ServiceWorkerRegisterJob::Start() {
  SetPhase(START);
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
  return REGISTRATION;
}

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    ServiceWorkerRegistration* registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration);
  internal_.registration = registration;
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() {
  DCHECK(phase_ >= REGISTER) << phase_;
  DCHECK(internal_.registration);
  return internal_.registration;
}

void ServiceWorkerRegisterJob::set_pending_version(
    ServiceWorkerVersion* version) {
  DCHECK(phase_ == UPDATE || phase_ == ACTIVATE) << phase_;
  DCHECK(!internal_.pending_version || !version);
  internal_.pending_version = version;
}

ServiceWorkerVersion* ServiceWorkerRegisterJob::pending_version() {
  DCHECK(phase_ >= UPDATE) << phase_;
  return internal_.pending_version;
}

void ServiceWorkerRegisterJob::SetPhase(Phase phase) {
  switch (phase) {
    case INITIAL:
      NOTREACHED();
      break;
    case START:
      DCHECK(phase_ == INITIAL) << phase_;
      break;
    case REGISTER:
      DCHECK(phase_ == START) << phase_;
      break;
    case UPDATE:
      DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
      break;
    case INSTALL:
      DCHECK(phase_ == UPDATE) << phase_;
      break;
    case ACTIVATE:
      DCHECK(phase_ == INSTALL) << phase_;
      break;
    case COMPLETE:
      DCHECK(phase_ != INITIAL && phase_ != COMPLETE) << phase_;
      break;
  }
  phase_ = phase;
}

// This function corresponds to the steps in Register following
// "Let serviceWorkerRegistration be _GetRegistration(scope)"
// |existing_registration| corresponds to serviceWorkerRegistration.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration) {
  // On unexpected error, abort this registration job.
  if (status != SERVICE_WORKER_ERROR_NOT_FOUND && status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "If serviceWorkerRegistration is not null and script is equal to
  // serviceWorkerRegistration.scriptUrl..." resolve with the existing
  // registration and abort.
  if (existing_registration.get() &&
      existing_registration->script_url() == script_url_) {
    set_registration(existing_registration);
    // If there's no active version, go ahead to Update (this isn't in the spec
    // but seems reasonable, and without SoftUpdate implemented we can never
    // Update otherwise).
    if (!existing_registration->active_version()) {
      UpdateAndContinue(status);
      return;
    }
    RunCallbacks(
        status, existing_registration, existing_registration->active_version());
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "If serviceWorkerRegistration is null..." create a new registration.
  if (!existing_registration.get()) {
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
  SetPhase(REGISTER);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  set_registration(new ServiceWorkerRegistration(
      pattern_, script_url_, context_->storage()->NewRegistrationId(),
      context_));
  context_->storage()->StoreRegistration(
      registration(),
      base::Bind(&ServiceWorkerRegisterJob::UpdateAndContinue,
                 weak_factory_.GetWeakPtr()));
}

// This function corresponds to the spec's _Update algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue(
    ServiceWorkerStatusCode status) {
  SetPhase(UPDATE);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  // TODO(falken): "If serviceWorkerRegistration.pendingWorker is not null..."
  // then terminate the pending worker. It doesn't make sense to implement yet
  // since we always activate the worker if install completed, so there can be
  // no pending worker at this point.
  DCHECK(!registration()->pending_version());

  // TODO(michaeln,falken): Script fetching and comparing the old and new
  // script belongs here.

  // "Let serviceWorker be a newly-created ServiceWorker object..." and start
  // the worker.
  set_pending_version(new ServiceWorkerVersion(
      registration(), context_->storage()->NewVersionId(), context_));
  for (std::vector<int>::const_iterator it = pending_process_ids_.begin();
       it != pending_process_ids_.end();
       ++it)
    pending_version()->AddProcessToWorker(*it);

  pending_version()->StartWorker(
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
  DCHECK(!registration()->pending_version());
  registration()->set_pending_version(pending_version());
  RunCallbacks(status, registration(), pending_version());

  // TODO(kinuko): Iterate over all provider hosts and call SetPendingVersion()
  // for documents that are in-scope.

  InstallAndContinue();
}

// This function corresponds to the spec's _Install algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  SetPhase(INSTALL);
  // "Set serviceWorkerRegistration.pendingWorker._state to installing."
  // "Fire install event on the associated ServiceWorkerGlobalScope object."
  pending_version()->DispatchInstallEvent(
      -1,
      base::Bind(&ServiceWorkerRegisterJob::OnInstallFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    ServiceWorkerStatusCode status) {
  // "If any handler called waitUntil()..." and the resulting promise
  // is rejected, abort.
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    registration()->set_pending_version(NULL);
    Complete(status);
    return;
  }

  ActivateAndContinue();
}

// This function corresponds to the spec's _Activate algorithm.
void ServiceWorkerRegisterJob::ActivateAndContinue() {
  SetPhase(ACTIVATE);

  // TODO(michaeln): Persist the newly ACTIVE version.

  // "If existingWorker is not null, then: wait for exitingWorker to finish
  // handling any in-progress requests."
  // See if we already have an active_version for the scope and it has
  // controllee documents (if so activating the new version should wait
  // until we have no documents controlled by the version).
  if (registration()->active_version() &&
      registration()->active_version()->HasControllee()) {
    // TODO(kinuko,falken): Currently we immediately return if the existing
    // registration already has an active version, so we shouldn't come
    // this way.
    NOTREACHED();
    // TODO(falken): Register an continuation task to wait for NoControllees
    // notification so that we can resume activation later (see comments
    // in ServiceWorkerVersion::RemoveControllee).
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "Set serviceWorkerRegistration.pendingWorker to null."
  // "Set serviceWorkerRegistration.activeWorker to activatingWorker."
  registration()->set_pending_version(NULL);
  DCHECK(!registration()->active_version());
  registration()->set_active_version(pending_version());

  // "Set serviceWorkerRegistration.activeWorker._state to activating."
  // "Fire activate event on the associated ServiceWorkerGlobalScope object."
  // "Set serviceWorkerRegistration.activeWorker._state to active."
  pending_version()->DispatchActivateEvent(
      base::Bind(&ServiceWorkerRegisterJob::OnActivateFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnActivateFinished(
    ServiceWorkerStatusCode status) {
  // "If any handler called waitUntil()..." and the resulting promise
  // is rejected, abort.
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    registration()->set_active_version(NULL);
    Complete(status);
    return;
  }

  set_pending_version(NULL);
  Complete(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  SetPhase(COMPLETE);
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
