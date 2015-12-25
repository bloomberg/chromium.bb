// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_router.h"

#include <string>

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace content {

namespace {

void RunDeliverCallback(
    const PushMessagingRouter::DeliverMessageCallback& deliver_message_callback,
    PushDeliveryStatus delivery_status) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(deliver_message_callback, delivery_status));
}

}  // namespace

// static
void PushMessagingRouter::DeliverMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& data,
    const DeliverMessageCallback& deliver_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PushMessagingRouter::FindServiceWorkerRegistration, origin,
                 service_worker_registration_id, data, deliver_message_callback,
                 service_worker_context));
}

// static
void PushMessagingRouter::FindServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& data,
    const DeliverMessageCallback& deliver_message_callback,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Try to acquire the registration from storage. If it's already live we'll
  // receive it right away. If not, it will be revived from storage.
  service_worker_context->FindReadyRegistrationForId(
      service_worker_registration_id, origin,
      base::Bind(&PushMessagingRouter::FindServiceWorkerRegistrationCallback,
                 data, deliver_message_callback));
}

// static
void PushMessagingRouter::FindServiceWorkerRegistrationCallback(
    const std::string& data,
    const DeliverMessageCallback& deliver_message_callback,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mvanouwerkerk): UMA logging.
  if (service_worker_status != SERVICE_WORKER_OK) {
    RunDeliverCallback(deliver_message_callback,
                       PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER);
    return;
  }

  ServiceWorkerVersion* version = service_worker_registration->active_version();
  DCHECK(version);

  // Hold on to the service worker registration in the callback to keep it
  // alive until the callback dies. Otherwise the registration could be
  // released when this method returns - before the event is delivered to the
  // service worker.
  base::Callback<void(ServiceWorkerStatusCode)> dispatch_event_callback =
      base::Bind(&PushMessagingRouter::DeliverMessageEnd,
                 deliver_message_callback, service_worker_registration);

  version->DispatchPushEvent(dispatch_event_callback, data);
}

// static
void PushMessagingRouter::DeliverMessageEnd(
    const DeliverMessageCallback& deliver_message_callback,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mvanouwerkerk): UMA logging.
  PushDeliveryStatus delivery_status =
      PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR;
  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      delivery_status = PUSH_DELIVERY_STATUS_SUCCESS;
      break;
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
      delivery_status = PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_NOT_FOUND:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
      delivery_status = PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR;
      break;
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      NOTREACHED() << "Got unexpected error code: " << service_worker_status
                   << " " << ServiceWorkerStatusToString(service_worker_status);
      delivery_status = PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR;
      break;
  }
  RunDeliverCallback(deliver_message_callback, delivery_status);
}

}  // namespace content
