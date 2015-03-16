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
    int64 registration_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : pattern_(pattern),
      registration_id_(registration_id),
      is_deleted_(false),
      is_uninstalling_(false),
      is_uninstalled_(false),
      should_activate_when_ready_(false),
      resources_total_size_bytes_(0),
      context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context_);
  context_->AddLiveRegistration(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listeners_.might_have_observers());
  if (context_)
    context_->RemoveLiveRegistration(registration_id_);
  if (active_version())
    active_version()->RemoveListener(this);
}

ServiceWorkerVersion* ServiceWorkerRegistration::GetNewestVersion() const {
  if (installing_version())
    return installing_version();
  if (waiting_version())
    return waiting_version();
  return active_version();
}

void ServiceWorkerRegistration::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerRegistration::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerRegistration::NotifyRegistrationFailed() {
  FOR_EACH_OBSERVER(Listener, listeners_, OnRegistrationFailed(this));
}

void ServiceWorkerRegistration::NotifyUpdateFound() {
  FOR_EACH_OBSERVER(Listener, listeners_, OnUpdateFound(this));
}

ServiceWorkerRegistrationInfo ServiceWorkerRegistration::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return ServiceWorkerRegistrationInfo(
      pattern(),
      registration_id_,
      GetVersionInfo(active_version_.get()),
      GetVersionInfo(waiting_version_.get()),
      GetVersionInfo(installing_version_.get()),
      resources_total_size_bytes_);
}

void ServiceWorkerRegistration::SetActiveVersion(
    ServiceWorkerVersion* version) {
  should_activate_when_ready_ = false;
  SetVersionInternal(version, &active_version_,
                     ChangedVersionAttributesMask::ACTIVE_VERSION);
}

void ServiceWorkerRegistration::SetWaitingVersion(
    ServiceWorkerVersion* version) {
  should_activate_when_ready_ = false;
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
  if (*data_member && *data_member == active_version_)
    active_version_->RemoveListener(this);
  *data_member = version;
  if (active_version_.get() && active_version_.get() == version)
    active_version_->AddListener(this);
  mask.add(change_flag);
  ServiceWorkerRegistrationInfo info = GetInfo();
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnVersionAttributesChanged(this, mask, info));
}

void ServiceWorkerRegistration::UnsetVersionInternal(
    ServiceWorkerVersion* version,
    ChangedVersionAttributesMask* mask) {
  DCHECK(version);
  if (installing_version_.get() == version) {
    installing_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  } else if (waiting_version_.get() == version) {
    waiting_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::WAITING_VERSION);
  } else if (active_version_.get() == version) {
    active_version_->RemoveListener(this);
    active_version_ = NULL;
    mask->add(ChangedVersionAttributesMask::ACTIVE_VERSION);
  }
}

void ServiceWorkerRegistration::ActivateWaitingVersionWhenReady() {
  DCHECK(waiting_version());
  should_activate_when_ready_ = true;

  if (!active_version() || !active_version()->HasControllee() ||
      waiting_version()->skip_waiting())
    ActivateWaitingVersion();
}

void ServiceWorkerRegistration::ClaimClients(const StatusCallback& callback) {
  DCHECK(context_);
  DCHECK(active_version());
  // TODO(xiang): Should better not hit the database http://crbug.com/454250.
  context_->storage()->GetRegistrationsForOrigin(
      pattern_.GetOrigin(),
      base::Bind(&ServiceWorkerRegistration::DidGetRegistrationsForClaimClients,
                 this, callback, active_version_));
}

void ServiceWorkerRegistration::ClearWhenReady() {
  DCHECK(context_);
  if (is_uninstalling_)
    return;
  is_uninstalling_ = true;

  context_->storage()->NotifyUninstallingRegistration(this);
  context_->storage()->DeleteRegistration(
      id(),
      pattern().GetOrigin(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  if (!active_version() || !active_version()->HasControllee())
    Clear();
}

void ServiceWorkerRegistration::AbortPendingClear(
    const StatusCallback& callback) {
  DCHECK(context_);
  if (!is_uninstalling()) {
    callback.Run(SERVICE_WORKER_OK);
    return;
  }
  is_uninstalling_ = false;
  context_->storage()->NotifyDoneUninstallingRegistration(this);

  scoped_refptr<ServiceWorkerVersion> most_recent_version =
      waiting_version() ? waiting_version() : active_version();
  DCHECK(most_recent_version.get());
  context_->storage()->NotifyInstallingRegistration(this);
  context_->storage()->StoreRegistration(
      this,
      most_recent_version.get(),
      base::Bind(&ServiceWorkerRegistration::OnRestoreFinished,
                 this,
                 callback,
                 most_recent_version));
}

void ServiceWorkerRegistration::GetUserData(
    const std::string& key,
    const GetUserDataCallback& callback) {
  DCHECK(context_);
  context_->storage()->GetUserData(registration_id_, key, callback);
}

void ServiceWorkerRegistration::StoreUserData(
    const std::string& key,
    const std::string& data,
    const StatusCallback& callback) {
  DCHECK(context_);
  context_->storage()->StoreUserData(
      registration_id_, pattern().GetOrigin(), key, data, callback);
}

void ServiceWorkerRegistration::ClearUserData(
    const std::string& key,
    const StatusCallback& callback) {
  DCHECK(context_);
  context_->storage()->ClearUserData(registration_id_, key, callback);
}

void ServiceWorkerRegistration::OnNoControllees(ServiceWorkerVersion* version) {
  DCHECK_EQ(active_version(), version);
  if (is_uninstalling_)
    Clear();
  else if (should_activate_when_ready_)
    ActivateWaitingVersion();
  is_uninstalling_ = false;
  should_activate_when_ready_ = false;
}

void ServiceWorkerRegistration::ActivateWaitingVersion() {
  DCHECK(context_);
  DCHECK(waiting_version());
  DCHECK(should_activate_when_ready_);
  should_activate_when_ready_ = false;
  scoped_refptr<ServiceWorkerVersion> activating_version = waiting_version();
  scoped_refptr<ServiceWorkerVersion> exiting_version = active_version();

  if (activating_version->is_doomed() ||
      activating_version->status() == ServiceWorkerVersion::REDUNDANT) {
    return;  // Activation is no longer relevant.
  }

  // "5. If exitingWorker is not null,
  if (exiting_version.get()) {
    // TODO(michaeln): should wait for events to be complete
    // "1. Wait for exitingWorker to finish handling any in-progress requests."
    // "2. Terminate exitingWorker."
    exiting_version->StopWorker(
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    // "3. Run the [[UpdateState]] algorithm passing exitingWorker and
    // "redundant" as the arguments."
    exiting_version->SetStatus(ServiceWorkerVersion::REDUNDANT);
  }

  // "6. Set serviceWorkerRegistration.activeWorker to activatingWorker."
  // "7. Set serviceWorkerRegistration.waitingWorker to null."
  SetActiveVersion(activating_version.get());

  // "8. Run the [[UpdateState]] algorithm passing registration.activeWorker and
  // "activating" as arguments."
  activating_version->SetStatus(ServiceWorkerVersion::ACTIVATING);
  // "9. Fire a simple event named controllerchange..."
  if (activating_version->skip_waiting())
    FOR_EACH_OBSERVER(Listener, listeners_, OnSkippedWaiting(this));

  // "10. Queue a task to fire an event named activate..."
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
    UnsetVersion(activating_version);
    activating_version->Doom();
    if (!waiting_version()) {
      // Delete the records from the db.
      context_->storage()->DeleteRegistration(
          id(), pattern().GetOrigin(),
          base::Bind(&ServiceWorkerRegistration::OnDeleteFinished, this));
      // But not from memory if there is a version in the pipeline.
      if (installing_version()) {
        is_deleted_ = false;
      } else {
        is_uninstalled_ = true;
        FOR_EACH_OBSERVER(Listener, listeners_, OnRegistrationFailed(this));
      }
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

void ServiceWorkerRegistration::OnDeleteFinished(
    ServiceWorkerStatusCode status) {
  // Intentionally empty completion callback, used to prevent
  // |this| from being deleted until the storage method completes.
}

void ServiceWorkerRegistration::Clear() {
  is_uninstalling_ = false;
  is_uninstalled_ = true;
  if (context_)
    context_->storage()->NotifyDoneUninstallingRegistration(this);

  ChangedVersionAttributesMask mask;
  if (installing_version_.get()) {
    installing_version_->Doom();
    installing_version_ = NULL;
    mask.add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  }
  if (waiting_version_.get()) {
    waiting_version_->Doom();
    waiting_version_ = NULL;
    mask.add(ChangedVersionAttributesMask::WAITING_VERSION);
  }
  if (active_version_.get()) {
    active_version_->Doom();
    active_version_->RemoveListener(this);
    active_version_ = NULL;
    mask.add(ChangedVersionAttributesMask::ACTIVE_VERSION);
  }
  if (mask.changed()) {
    ServiceWorkerRegistrationInfo info = GetInfo();
    FOR_EACH_OBSERVER(Listener, listeners_,
                      OnVersionAttributesChanged(this, mask, info));
  }

  FOR_EACH_OBSERVER(
      Listener, listeners_, OnRegistrationFinishedUninstalling(this));
}

void ServiceWorkerRegistration::OnRestoreFinished(
    const StatusCallback& callback,
    scoped_refptr<ServiceWorkerVersion> version,
    ServiceWorkerStatusCode status) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  context_->storage()->NotifyDoneInstallingRegistration(
      this, version.get(), status);
  callback.Run(status);
}

void ServiceWorkerRegistration::DidGetRegistrationsForClaimClients(
    const StatusCallback& callback,
    scoped_refptr<ServiceWorkerVersion> version,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  if (!active_version() || version != active_version()) {
    callback.Run(SERVICE_WORKER_ERROR_STATE);
    return;
  }

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context_->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (ShouldClaim(host, registrations))
      host->ClaimedByRegistration(this);
  }
  callback.Run(SERVICE_WORKER_OK);
}

bool ServiceWorkerRegistration::ShouldClaim(
    ServiceWorkerProviderHost* provider_host,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  if (provider_host->IsHostToRunningServiceWorker())
    return false;
  if (provider_host->controlling_version() == active_version())
    return false;

  LongestScopeMatcher matcher(provider_host->document_url());
  if (!matcher.MatchLongest(pattern_))
    return false;
  for (const ServiceWorkerRegistrationInfo& info : registrations) {
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(info.registration_id);
    if (registration &&
        (registration->is_uninstalling() || registration->is_uninstalled()))
      continue;
    if (matcher.MatchLongest(info.pattern))
      return false;
  }
  return true;
}

}  // namespace content
