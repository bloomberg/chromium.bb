// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

}  // namespace

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& pattern,
    const GURL& script_url)
    : context_(context),
      job_type_(REGISTRATION_JOB),
      pattern_(pattern),
      script_url_(script_url),
      phase_(INITIAL),
      doom_installing_worker_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      promise_resolved_status_(SERVICE_WORKER_OK),
      weak_factory_(this) {
}

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRegistration* registration)
    : context_(context),
      job_type_(UPDATE_JOB),
      pattern_(registration->pattern()),
      script_url_(registration->GetNewestVersion()->script_url()),
      phase_(INITIAL),
      doom_installing_worker_(false),
      is_promise_resolved_(false),
      should_uninstall_on_failure_(false),
      promise_resolved_status_(SERVICE_WORKER_OK),
      weak_factory_(this) {
  internal_.registration = registration;
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(!context_ ||
         phase_ == INITIAL || phase_ == COMPLETE || phase_ == ABORT)
      << "Jobs should only be interrupted during shutdown.";
}

void ServiceWorkerRegisterJob::AddCallback(
    const RegistrationCallback& callback,
    ServiceWorkerProviderHost* provider_host) {
  if (!is_promise_resolved_) {
    callbacks_.push_back(callback);
    if (provider_host)
      provider_host->AddScopedProcessReferenceToPattern(pattern_);
    return;
  }
  RunSoon(base::Bind(callback, promise_resolved_status_,
                     promise_resolved_status_message_,
                     promise_resolved_registration_));
}

void ServiceWorkerRegisterJob::Start() {
  SetPhase(START);
  ServiceWorkerStorage::FindRegistrationCallback next_step;
  if (job_type_ == REGISTRATION_JOB) {
    next_step = base::Bind(
        &ServiceWorkerRegisterJob::ContinueWithRegistration,
        weak_factory_.GetWeakPtr());
  } else {
    next_step = base::Bind(
        &ServiceWorkerRegisterJob::ContinueWithUpdate,
        weak_factory_.GetWeakPtr());
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->storage()->GetUninstallingRegistration(pattern_);
  if (registration.get())
    RunSoon(base::Bind(next_step, SERVICE_WORKER_OK, registration));
  else
    context_->storage()->FindRegistrationForPattern(pattern_, next_step);
}

void ServiceWorkerRegisterJob::Abort() {
  SetPhase(ABORT);
  CompleteInternal(SERVICE_WORKER_ERROR_ABORT, std::string());
  // Don't have to call FinishJob() because the caller takes care of removing
  // the jobs from the queue.
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJobBase* job) const {
  if (job->GetType() != GetType())
    return false;
  ServiceWorkerRegisterJob* register_job =
      static_cast<ServiceWorkerRegisterJob*>(job);
  return register_job->pattern_ == pattern_ &&
         register_job->script_url_ == script_url_;
}

RegistrationJobType ServiceWorkerRegisterJob::GetType() const {
  return job_type_;
}

void ServiceWorkerRegisterJob::DoomInstallingWorker() {
  doom_installing_worker_ = true;
  if (phase_ == INSTALL)
    Complete(SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED, std::string());
}

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration.get());
  internal_.registration = registration;
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() {
  DCHECK(phase_ >= REGISTER || job_type_ == UPDATE_JOB) << phase_;
  return internal_.registration.get();
}

void ServiceWorkerRegisterJob::set_new_version(
    ServiceWorkerVersion* version) {
  DCHECK(phase_ == UPDATE) << phase_;
  DCHECK(!internal_.new_version.get());
  internal_.new_version = version;
}

ServiceWorkerVersion* ServiceWorkerRegisterJob::new_version() {
  DCHECK(phase_ >= UPDATE) << phase_;
  return internal_.new_version.get();
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
    case STORE:
      DCHECK(phase_ == INSTALL) << phase_;
      break;
    case COMPLETE:
      DCHECK(phase_ != INITIAL && phase_ != COMPLETE) << phase_;
      break;
    case ABORT:
      break;
  }
  phase_ = phase;
}

// This function corresponds to the steps in [[Register]] following
// "Let registration be the result of running the [[GetRegistration]] algorithm.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::ContinueWithRegistration(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration) {
  DCHECK_EQ(REGISTRATION_JOB, job_type_);
  if (status != SERVICE_WORKER_ERROR_NOT_FOUND && status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  if (!existing_registration.get() || existing_registration->is_uninstalled()) {
    RegisterAndContinue();
    return;
  }

  DCHECK(existing_registration->GetNewestVersion());
  // "If scriptURL is equal to registration.[[ScriptURL]], then:"
  if (existing_registration->GetNewestVersion()->script_url() == script_url_) {
    // "Set registration.[[Uninstalling]] to false."
    existing_registration->AbortPendingClear(base::Bind(
        &ServiceWorkerRegisterJob::ContinueWithRegistrationForSameScriptUrl,
        weak_factory_.GetWeakPtr(),
        existing_registration));
    return;
  }

  if (existing_registration->is_uninstalling()) {
    existing_registration->AbortPendingClear(base::Bind(
        &ServiceWorkerRegisterJob::ContinueWithUninstallingRegistration,
        weak_factory_.GetWeakPtr(),
        existing_registration));
    return;
  }

  // "Return the result of running the [[Update]] algorithm, or its equivalent,
  // passing registration as the argument."
  set_registration(existing_registration);
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithUpdate(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration) {
  DCHECK_EQ(UPDATE_JOB, job_type_);
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  if (existing_registration.get() != registration()) {
    Complete(SERVICE_WORKER_ERROR_NOT_FOUND);
    return;
  }

  // A previous job may have unregistered or installed a new version to this
  // registration.
  if (registration()->is_uninstalling() ||
      registration()->GetNewestVersion()->script_url() != script_url_) {
    Complete(SERVICE_WORKER_ERROR_NOT_FOUND);
    return;
  }

  // TODO(michaeln): If the last update check was less than 24 hours
  // ago, depending on the freshness of the cached worker script we
  // may be able to complete the update job right here.

  UpdateAndContinue();
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue() {
  SetPhase(REGISTER);

  set_registration(new ServiceWorkerRegistration(
      pattern_, context_->storage()->NewRegistrationId(), context_));
  AssociateProviderHostsToRegistration(registration());
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithUninstallingRegistration(
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }
  should_uninstall_on_failure_ = true;
  set_registration(existing_registration);
  UpdateAndContinue();
}

void ServiceWorkerRegisterJob::ContinueWithRegistrationForSameScriptUrl(
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }
  set_registration(existing_registration);

  // "If newestWorker is not null, and scriptURL is equal to
  // newestWorker.scriptURL, then:
  // Return a promise resolved with registration."
  // We resolve only if there's an active version. If there's not,
  // then there is either no version or only a waiting version from
  // the last browser session; it makes sense to proceed with registration in
  // either case.
  DCHECK(!existing_registration->installing_version());
  if (existing_registration->active_version()) {
    ResolvePromise(status, std::string(), existing_registration.get());
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "Return the result of running the [[Update]] algorithm, or its equivalent,
  // passing registration as the argument."
  UpdateAndContinue();
}

// This function corresponds to the spec's [[Update]] algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue() {
  SetPhase(UPDATE);
  context_->storage()->NotifyInstallingRegistration(registration());

  // "Let worker be a new ServiceWorker object..." and start
  // the worker.
  set_new_version(new ServiceWorkerVersion(registration(),
                                           script_url_,
                                           context_->storage()->NewVersionId(),
                                           context_));

  bool pause_after_download = job_type_ == UPDATE_JOB;
  if (pause_after_download)
    new_version()->embedded_worker()->AddListener(this);
  new_version()->StartWorker(
      pause_after_download,
      base::Bind(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    ServiceWorkerStatusCode status) {
  if (status == SERVICE_WORKER_OK) {
    InstallAndContinue();
    return;
  }

  // "If serviceWorker fails to start up..." then reject the promise with an
  // error and abort. When there is a main script network error, the status will
  // be updated to a more specific one.
  const net::URLRequestStatus& main_script_status =
      new_version()->script_cache_map()->main_script_status();
  std::string message;
  if (main_script_status.status() != net::URLRequestStatus::SUCCESS) {
    switch (main_script_status.error()) {
      case net::ERR_INSECURE_RESPONSE:
      case net::ERR_UNSAFE_REDIRECT:
        status = SERVICE_WORKER_ERROR_SECURITY;
        break;
      case net::ERR_ABORTED:
        status = SERVICE_WORKER_ERROR_ABORT;
        break;
      default:
        status = SERVICE_WORKER_ERROR_NETWORK;
    }
    message = new_version()->script_cache_map()->main_script_status_message();
    if (message.empty())
      message = kFetchScriptError;
  }

  if (status == SERVICE_WORKER_ERROR_TIMEOUT)
    message = "Timed out while trying to start the Service Worker.";

  Complete(status, message);
}

// This function corresponds to the spec's [[Install]] algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  SetPhase(INSTALL);

  // "Set registration.installingWorker to worker."
  DCHECK(!registration()->installing_version());
  registration()->SetInstallingVersion(new_version());

  // "Run the Update State algorithm passing registration's installing worker
  // and installing as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLING);

  // "Resolve registrationPromise with registration."
  ResolvePromise(SERVICE_WORKER_OK, std::string(), registration());

  // "Fire a simple event named updatefound..."
  registration()->NotifyUpdateFound();

  // "Fire an event named install..."
  new_version()->DispatchInstallEvent(
      base::Bind(&ServiceWorkerRegisterJob::OnInstallFinished,
                 weak_factory_.GetWeakPtr()));

  // A subsequent registration job may terminate our installing worker. It can
  // only do so after we've started the worker and dispatched the install
  // event, as those are atomic substeps in the [[Install]] algorithm.
  if (doom_installing_worker_)
    Complete(SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    // "8. If installFailed is true, then:..."
    Complete(status);
    return;
  }

  SetPhase(STORE);
  registration()->set_last_update_check(base::Time::Now());
  context_->storage()->StoreRegistration(
      registration(),
      new_version(),
      base::Bind(&ServiceWorkerRegisterJob::OnStoreRegistrationComplete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStoreRegistrationComplete(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "9. If registration.waitingWorker is not null, then:..."
  if (registration()->waiting_version()) {
    // "1. Run the [[UpdateState]] algorithm passing registration.waitingWorker
    // and "redundant" as the arguments."
    registration()->waiting_version()->SetStatus(
        ServiceWorkerVersion::REDUNDANT);
  }

  // "10. Set registration.waitingWorker to registration.installingWorker."
  // "11. Set registration.installingWorker to null."
  registration()->SetWaitingVersion(new_version());

  // "12. Run the [[UpdateState]] algorithm passing registration.waitingWorker
  // and "installed" as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLED);

  // "If registration's waiting worker's skip waiting flag is set:" then
  // activate the worker immediately otherwise "wait until no service worker
  // client is using registration as their service worker registration."
  registration()->ActivateWaitingVersionWhenReady();

  Complete(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  Complete(status, std::string());
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status,
                                        const std::string& status_message) {
  CompleteInternal(status, status_message);
  context_->job_coordinator()->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::CompleteInternal(
    ServiceWorkerStatusCode status,
    const std::string& status_message) {
  SetPhase(COMPLETE);
  if (status != SERVICE_WORKER_OK) {
    if (registration()) {
      if (should_uninstall_on_failure_)
        registration()->ClearWhenReady();
      if (new_version()) {
        registration()->UnsetVersion(new_version());
        new_version()->Doom();
      }
      if (!registration()->waiting_version() &&
          !registration()->active_version()) {
        registration()->NotifyRegistrationFailed();
        context_->storage()->DeleteRegistration(
            registration()->id(),
            registration()->pattern().GetOrigin(),
            base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
      }
    }
    if (!is_promise_resolved_)
      ResolvePromise(status, status_message, NULL);
  }
  DCHECK(callbacks_.empty());
  if (registration()) {
    context_->storage()->NotifyDoneInstallingRegistration(
        registration(), new_version(), status);
  }
  if (new_version())
    new_version()->embedded_worker()->RemoveListener(this);
}

void ServiceWorkerRegisterJob::ResolvePromise(
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  DCHECK(!is_promise_resolved_);

  is_promise_resolved_ = true;
  promise_resolved_status_ = status;
  promise_resolved_status_message_ = status_message,
  promise_resolved_registration_ = registration;
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, status_message, registration);
  }
  callbacks_.clear();
}

void ServiceWorkerRegisterJob::OnPausedAfterDownload() {
  // This happens prior to OnStartWorkerFinished time.
  scoped_refptr<ServiceWorkerVersion> most_recent_version =
      registration()->waiting_version() ?
          registration()->waiting_version() :
          registration()->active_version();
  DCHECK(most_recent_version.get());
  int64 most_recent_script_id =
      most_recent_version->script_cache_map()->LookupResourceId(script_url_);
  int64 new_script_id =
      new_version()->script_cache_map()->LookupResourceId(script_url_);

  // TODO(michaeln): It would be better to compare as the new resource
  // is being downloaded and to avoid writing it to disk until we know
  // its needed.
  context_->storage()->CompareScriptResources(
      most_recent_script_id,
      new_script_id,
      base::Bind(&ServiceWorkerRegisterJob::OnCompareScriptResourcesComplete,
                 weak_factory_.GetWeakPtr()));
}

bool ServiceWorkerRegisterJob::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void ServiceWorkerRegisterJob::OnCompareScriptResourcesComplete(
    ServiceWorkerStatusCode status,
    bool are_equal) {
  if (are_equal) {
    // Only bump the last check time when we've bypassed the browser cache.
    base::TimeDelta time_since_last_check =
        base::Time::Now() - registration()->last_update_check();
    if (time_since_last_check > base::TimeDelta::FromHours(24)) {
      registration()->set_last_update_check(base::Time::Now());
      context_->storage()->UpdateLastUpdateCheckTime(registration());
    }

    ResolvePromise(SERVICE_WORKER_OK, std::string(), registration());
    Complete(SERVICE_WORKER_ERROR_EXISTS);
    return;
  }

  // Proceed with really starting the worker.
  new_version()->embedded_worker()->ResumeAfterDownload();
  new_version()->embedded_worker()->RemoveListener(this);
}

void ServiceWorkerRegisterJob::AssociateProviderHostsToRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context_->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (ServiceWorkerUtils::ScopeMatches(registration->pattern(),
                                         host->document_url())) {
      if (host->CanAssociateRegistration(registration))
        host->AssociateRegistration(registration);
    }
  }
}

}  // namespace content
