// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

ServiceWorkerVersionInfo GetVersionInfo(ServiceWorkerVersion* version) {
  if (!version)
    return ServiceWorkerVersionInfo();
  return version->GetInfo();
}

}  // namespace

ServiceWorkerRegistration::ServiceWorkerRegistration(
    const GURL& pattern,
    const GURL& script_url,
    int64 registration_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : pattern_(pattern),
      script_url_(script_url),
      registration_id_(registration_id),
      is_deleted_(false),
      should_activate_when_ready_(false),
      context_(context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context_);
  context_->AddLiveRegistration(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (context_)
    context_->RemoveLiveRegistration(registration_id_);
  ResetShouldActivateWhenReady();
}

void ServiceWorkerRegistration::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerRegistration::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

ServiceWorkerRegistrationInfo ServiceWorkerRegistration::GetInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ServiceWorkerRegistrationInfo(
      script_url(),
      pattern(),
      registration_id_,
      GetVersionInfo(active_version_),
      GetVersionInfo(waiting_version_),
      GetVersionInfo(installing_version_));
}

void ServiceWorkerRegistration::SetActiveVersion(
    ServiceWorkerVersion* version) {
  ResetShouldActivateWhenReady();
  SetVersionInternal(version, &active_version_,
                     ChangedVersionAttributesMask::ACTIVE_VERSION);
}

void ServiceWorkerRegistration::SetWaitingVersion(
    ServiceWorkerVersion* version) {
  ResetShouldActivateWhenReady();
  SetVersionInternal(version, &waiting_version_,
                     ChangedVersionAttributesMask::WAITING_VERSION);
}

void ServiceWorkerRegistration::SetInstallingVersion(
    ServiceWorkerVersion* version) {
  SetVersionInternal(version, &installing_version_,
                     ChangedVersionAttributesMask::INSTALLING_VERSION);
}

void ServiceWorkerRegistration::UnsetVersion(ServiceWorkerVersion* version) {
  if (!version)
    return;
  ChangedVersionAttributesMask mask;
  UnsetVersionInternal(version, &mask);
  if (mask.changed()) {
    ServiceWorkerRegistrationInfo info = GetInfo();
    FOR_EACH_OBSERVER(Listener, listeners_,
                      OnVersionAttributesChanged(this, mask, info));
  }
}

void ServiceWorkerRegistration::SetVersionInternal(
    ServiceWorkerVersion* version,
    scoped_refptr<ServiceWorkerVersion>* data_member,
    int change_flag) {
  if (version == data_member->get())
    return;
  scoped_refptr<ServiceWorkerVersion> protect(version);
  ChangedVersionAttributesMask mask;
  if (version)
    UnsetVersionInternal(version, &mask);
  *data_member = version;
  mask.add(change_flag);
  ServiceWorkerRegistrationInfo info = GetInfo();
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnVersionAttributesChanged(this, mask, info));
}

void ServiceWorkerRegistration::UnsetVersionInternal(
    ServiceWorkerVersion* version,
    ChangedVersionAttributesMask* mask) {
  DCHECK(version);
  if (installing_version_ == version) {
    installing_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  } else if (waiting_version_  == version) {
    waiting_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::WAITING_VERSION);
  } else if (active_version_ == version) {
    active_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::ACTIVE_VERSION);
  }
}

void ServiceWorkerRegistration::ActivateWaitingVersionWhenReady() {
  DCHECK(waiting_version());
  if (should_activate_when_ready_)
    return;
  if (active_version() && active_version()->HasControllee()) {
    active_version()->AddListener(this);
    should_activate_when_ready_ = true;
    return;
  }
  ActivateWaitingVersion();
}

void ServiceWorkerRegistration::OnNoControllees(ServiceWorkerVersion* version) {
  DCHECK_EQ(active_version(), version);
  DCHECK(should_activate_when_ready_);
  active_version_->RemoveListener(this);
  should_activate_when_ready_ = false;
  ActivateWaitingVersion();
}

void ServiceWorkerRegistration::ActivateWaitingVersion() {
  DCHECK(context_);
  DCHECK(waiting_version());
  scoped_refptr<ServiceWorkerVersion> activating_version = waiting_version();
  scoped_refptr<ServiceWorkerVersion> exiting_version = active_version();

  if (activating_version->is_doomed() ||
      activating_version->status() == ServiceWorkerVersion::REDUNDANT) {
    return;  // Activation is no longer relevant.
  }

  // "4. If exitingWorker is not null,
  if (exiting_version) {
    DCHECK(!exiting_version->HasControllee());
    // TODO(michaeln): should wait for events to be complete
    // "1. Wait for exitingWorker to finish handling any in-progress requests."
    // "2. Terminate exitingWorker."
    exiting_version->StopWorker(
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    // "3. Run the [[UpdateState]] algorithm passing exitingWorker and
    // "redundant" as the arguments."
    exiting_version->SetStatus(ServiceWorkerVersion::REDUNDANT);
  }

  // "5. Set serviceWorkerRegistration.activeWorker to activatingWorker."
  // "6. Set serviceWorkerRegistration.waitingWorker to null."
  ServiceWorkerRegisterJob::DisassociateVersionFromDocuments(
      context_, activating_version);
  SetActiveVersion(activating_version);
  ServiceWorkerRegisterJob::AssociateActiveVersionToDocuments(
      context_, activating_version);

  // "7. Run the [[UpdateState]] algorithm passing registration.activeWorker and
  // "activating" as arguments."
  activating_version->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // TODO(nhiroki): "8. Fire a simple event named controllerchange..."

  // "9. Queue a task to fire an event named activate..."
  activating_version->DispatchActivateEvent(
      base::Bind(&ServiceWorkerRegistration::OnActivateEventFinished,
                 this, activating_version));
}

void ServiceWorkerRegistration::OnActivateEventFinished(
    ServiceWorkerVersion* activating_version,
    ServiceWorkerStatusCode status) {
  if (!context_ || activating_version != active_version())
    return;
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    // "11. If activateFailed is true, then:..."
    ServiceWorkerRegisterJob::DisassociateVersionFromDocuments(
        context_, activating_version);
    UnsetVersion(activating_version);
    activating_version->Doom();
    if (!waiting_version()) {
      // Delete the records from the db.
      context_->storage()->DeleteRegistration(
          id(), script_url().GetOrigin(),
          base::Bind(&ServiceWorkerRegistration::OnDeleteFinished, this));
      // But not from memory if there is a version in the pipeline.
      if (installing_version())
        is_deleted_ = false;
    }
    return;
  }

  // "12. Run the [[UpdateState]] algorithm passing registration.activeWorker
  // and "activated" as the arguments."
  activating_version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  if (context_) {
    context_->storage()->UpdateToActiveState(
        this,
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  }
}

void ServiceWorkerRegistration::ResetShouldActivateWhenReady() {
  if (should_activate_when_ready_) {
    active_version()->RemoveListener(this);
    should_activate_when_ready_ = false;
  }
}

void ServiceWorkerRegistration::OnDeleteFinished(
    ServiceWorkerStatusCode status) {
  // Intentionally empty completion callback, used to prevent
  // |this| from being deleted until the storage method completes.
}

}  // namespace content
