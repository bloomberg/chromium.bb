// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_handle.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerRegistrationHandle::ServiceWorkerRegistrationHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host,
    int provider_id,
    ServiceWorkerRegistration* registration)
    : context_(context),
      dispatcher_host_(dispatcher_host),
      provider_id_(provider_id),
      handle_id_(context ? context->GetNewRegistrationHandleId()
                         : kInvalidServiceWorkerRegistrationHandleId),
      ref_count_(1),
      registration_(registration) {
  DCHECK(registration_.get());
  SetVersionAttributes(registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
  registration_->AddListener(this);
}

ServiceWorkerRegistrationHandle::~ServiceWorkerRegistrationHandle() {
  DCHECK(registration_.get());
  registration_->RemoveListener(this);
}

ServiceWorkerRegistrationObjectInfo
ServiceWorkerRegistrationHandle::GetObjectInfo() {
  ServiceWorkerRegistrationObjectInfo info;
  info.handle_id = handle_id_;
  info.scope = registration_->pattern();
  info.registration_id = registration_->id();
  return info;
}

void ServiceWorkerRegistrationHandle::IncrementRefCount() {
  DCHECK_GT(ref_count_, 0);
  ++ref_count_;
}

void ServiceWorkerRegistrationHandle::DecrementRefCount() {
  DCHECK_GT(ref_count_, 0);
  --ref_count_;
}

void ServiceWorkerRegistrationHandle::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_EQ(registration->id(), registration_->id());
  SetVersionAttributes(registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
}

void ServiceWorkerRegistrationHandle::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(registration->id(), registration_->id());
  ClearVersionAttributes();
}

void ServiceWorkerRegistrationHandle::OnUpdateFound(
    ServiceWorkerRegistration* registration) {
  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.
  dispatcher_host_->Send(new ServiceWorkerMsg_UpdateFound(
      kDocumentMainThreadId, GetObjectInfo()));
}

void ServiceWorkerRegistrationHandle::SetVersionAttributes(
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  ChangedVersionAttributesMask mask;

  if (installing_version != installing_version_.get()) {
    installing_version_ = installing_version;
    mask.add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  }
  if (waiting_version != waiting_version_.get()) {
    waiting_version_ = waiting_version;
    mask.add(ChangedVersionAttributesMask::WAITING_VERSION);
  }
  if (active_version != active_version_.get()) {
    active_version_ = active_version;
    mask.add(ChangedVersionAttributesMask::ACTIVE_VERSION);
  }

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.
  if (!mask.changed())
    return;

  ServiceWorkerVersionAttributes attributes;
  if (mask.installing_changed()) {
    attributes.installing =
        dispatcher_host_->CreateAndRegisterServiceWorkerHandle(
            installing_version);
  }
  if (mask.waiting_changed()) {
    attributes.waiting =
        dispatcher_host_->CreateAndRegisterServiceWorkerHandle(waiting_version);
  }
  if (mask.active_changed()) {
    attributes.active =
        dispatcher_host_->CreateAndRegisterServiceWorkerHandle(active_version);
  }

  dispatcher_host_->Send(new ServiceWorkerMsg_SetVersionAttributes(
      kDocumentMainThreadId, provider_id_, handle_id_,
      mask.changed(), attributes));
}

void ServiceWorkerRegistrationHandle::ClearVersionAttributes() {
  SetVersionAttributes(NULL, NULL, NULL);
}

}  // namespace content
