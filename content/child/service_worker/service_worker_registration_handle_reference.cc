// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_registration_handle_reference.h"

#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

scoped_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Create(
    const ServiceWorkerRegistrationObjectInfo& info,
    ThreadSafeSender* sender) {
  return make_scoped_ptr(new ServiceWorkerRegistrationHandleReference(
      info, sender, true));
}

scoped_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Adopt(
    const ServiceWorkerRegistrationObjectInfo& info,
    ThreadSafeSender* sender) {
  return make_scoped_ptr(new ServiceWorkerRegistrationHandleReference(
      info, sender, false));
}

ServiceWorkerRegistrationHandleReference::
ServiceWorkerRegistrationHandleReference(
    const ServiceWorkerRegistrationObjectInfo& info,
    ThreadSafeSender* sender,
    bool increment_ref_in_ctor)
    : info_(info),
      sender_(sender) {
  DCHECK_NE(kInvalidServiceWorkerRegistrationHandleId, info_.handle_id);
  DCHECK(sender_);
  if (increment_ref_in_ctor)
    return;
  sender_->Send(
      new ServiceWorkerHostMsg_IncrementRegistrationRefCount(info_.handle_id));
}

ServiceWorkerRegistrationHandleReference::
~ServiceWorkerRegistrationHandleReference() {
  sender_->Send(
      new ServiceWorkerHostMsg_DecrementRegistrationRefCount(info_.handle_id));
}

}  // namespace content
