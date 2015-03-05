// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include "base/callback.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/platform_notification_data.h"

namespace content {
namespace {

using NotificationClickDispatchCompleteCallback =
    NotificationEventDispatcher::NotificationClickDispatchCompleteCallback;

// To be called when the notificationclick event has finished executing. Will
// post a task to call |dispatch_complete_callback| on the UI thread.
void NotificationClickEventFinished(
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PersistentNotificationStatus status = PERSISTENT_NOTIFICATION_STATUS_SUCCESS;
  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      // Success status was initialized above.
      break;
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
      status = PERSISTENT_NOTIFICATION_STATUS_EVENT_WAITUNTIL_REJECTED;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
  }

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(dispatch_complete_callback, status));
}

// Dispatches the notificationclick on |service_worker_registration| if the
// registration was available. Must be called on the IO thread.
void DispatchNotificationClickEventOnRegistration(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    base::Callback<void(ServiceWorkerStatusCode)> dispatch_event_callback =
        base::Bind(&NotificationClickEventFinished,
                   dispatch_complete_callback,
                   service_worker_registration);
    service_worker_registration->active_version()
        ->DispatchNotificationClickEvent(dispatch_event_callback,
                                         notification_id,
                                         notification_data);
    return;
  }

  PersistentNotificationStatus status = PERSISTENT_NOTIFICATION_STATUS_SUCCESS;
  switch (service_worker_status) {
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      status = PERSISTENT_NOTIFICATION_STATUS_NO_SERVICE_WORKER;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
    case SERVICE_WORKER_OK:
      NOTREACHED();
      break;
  }

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(dispatch_complete_callback, status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the IO thread.
void FindServiceWorkerRegistration(
    const GURL& origin,
    int64 service_worker_registration_id,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context->context()->storage()->FindRegistrationForId(
      service_worker_registration_id,
      origin,
      base::Bind(&DispatchNotificationClickEventOnRegistration,
                 notification_id,
                 notification_data,
                 dispatch_complete_callback));
}

}  // namespace

// static
NotificationEventDispatcher* NotificationEventDispatcher::GetInstance() {
  return NotificationEventDispatcherImpl::GetInstance();
}

NotificationEventDispatcherImpl*
NotificationEventDispatcherImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return Singleton<NotificationEventDispatcherImpl>::get();
}

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() {}

NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() {}

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    const GURL& origin,
    int64 service_worker_registration_id,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    const NotificationClickDispatchCompleteCallback&
        dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&FindServiceWorkerRegistration,
                 origin,
                 service_worker_registration_id,
                 notification_id,
                 notification_data,
                 dispatch_complete_callback,
                 service_worker_context));
}

}  // namespace content
