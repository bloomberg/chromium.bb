// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_registration_handle_reference.h"

#include "base/memory/ptr_util.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

std::unique_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Create(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    ThreadSafeSender* sender) {
  return base::WrapUnique(new ServiceWorkerRegistrationHandleReference(
      std::move(info), sender, true));
}

std::unique_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerRegistrationHandleReference::Adopt(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    ThreadSafeSender* sender) {
  return base::WrapUnique(new ServiceWorkerRegistrationHandleReference(
      std::move(info), sender, false));
}

ServiceWorkerRegistrationHandleReference::
    ServiceWorkerRegistrationHandleReference(
        blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
        ThreadSafeSender* sender,
        bool increment_ref_in_ctor)
    : info_(std::move(info)), sender_(sender) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
            info_->handle_id);
  DCHECK(sender_.get());
  if (!increment_ref_in_ctor)
    return;
  sender_->Send(
      new ServiceWorkerHostMsg_IncrementRegistrationRefCount(info_->handle_id));
}

ServiceWorkerRegistrationHandleReference::
~ServiceWorkerRegistrationHandleReference() {
  sender_->Send(
      new ServiceWorkerHostMsg_DecrementRegistrationRefCount(info_->handle_id));
}

}  // namespace content
