// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_registration_handle_reference.h"

#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

scoped_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Create(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info,
    ThreadSafeSender* sender) {
  return make_scoped_ptr(new ServiceWorkerRegistrationHandleReference(
      registration_handle_id, info, sender, true));
}

scoped_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Adopt(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info,
    ThreadSafeSender* sender) {
  return make_scoped_ptr(new ServiceWorkerRegistrationHandleReference(
      registration_handle_id, info, sender, false));
}

ServiceWorkerRegistrationHandleReference::
ServiceWorkerRegistrationHandleReference(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info,
    ThreadSafeSender* sender,
    bool increment_ref_in_ctor)
    : handle_id_(registration_handle_id),
      scope_(info.scope),
      sender_(sender) {
  DCHECK_NE(kInvalidServiceWorkerRegistrationHandleId, handle_id_);
  DCHECK(sender_);
  if (increment_ref_in_ctor)
    return;
  sender_->Send(
      new ServiceWorkerHostMsg_IncrementRegistrationRefCount(handle_id_));
}

ServiceWorkerRegistrationHandleReference::
~ServiceWorkerRegistrationHandleReference() {
  sender_->Send(
      new ServiceWorkerHostMsg_DecrementRegistrationRefCount(handle_id_));
}

}  // namespace content
