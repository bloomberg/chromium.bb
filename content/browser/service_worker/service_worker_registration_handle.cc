// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_handle.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

static const int kDocumentMainThreadId = 0;

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
  DCHECK(registration_);
  SetVersionAttributes(registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
  registration_->AddListener(this);
}

ServiceWorkerRegistrationHandle::~ServiceWorkerRegistrationHandle() {
  DCHECK(registration_);
  registration_->RemoveListener(this);
}

ServiceWorkerRegistrationObjectInfo
ServiceWorkerRegistrationHandle::GetObjectInfo() {
  ServiceWorkerRegistrationObjectInfo info;
  info.handle_id = handle_id_;
  info.scope = registration_->pattern();
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

void ServiceWorkerRegistrationHandle::SetVersionAttributes(
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  ChangedVersionAttributesMask mask;

  if (installing_version != installing_version_) {
    installing_version_ = installing_version;
    mask.add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  }
  if (waiting_version != waiting_version_) {
    waiting_version_ = waiting_version;
    mask.add(ChangedVersionAttributesMask::WAITING_VERSION);
  }
  if (active_version != active_version_) {
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
        CreateServiceWorkerHandleAndPass(installing_version);
  }
  if (mask.waiting_changed()) {
    attributes.waiting =
        CreateServiceWorkerHandleAndPass(waiting_version);
  }
  if (mask.active_changed()) {
    attributes.active =
        CreateServiceWorkerHandleAndPass(active_version);
  }

  dispatcher_host_->Send(new ServiceWorkerMsg_SetVersionAttributes(
      kDocumentMainThreadId, provider_id_, handle_id_,
      mask.changed(), attributes));
}

void ServiceWorkerRegistrationHandle::ClearVersionAttributes() {
  SetVersionAttributes(NULL, NULL, NULL);
}

ServiceWorkerObjectInfo
ServiceWorkerRegistrationHandle::CreateServiceWorkerHandleAndPass(
    ServiceWorkerVersion* version) {
  ServiceWorkerObjectInfo info;
  if (context_ && version) {
    scoped_ptr<ServiceWorkerHandle> handle =
        ServiceWorkerHandle::Create(context_,
                                    dispatcher_host_,
                                    kDocumentMainThreadId,
                                    provider_id_,
                                    version);
    info = handle->GetObjectInfo();
    dispatcher_host_->RegisterServiceWorkerHandle(handle.Pass());
  }
  return info;
}

}  // namespace content
