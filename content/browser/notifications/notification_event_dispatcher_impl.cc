// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
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

#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "The notificationclick event has finished: "
            << service_worker_status;
#endif

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
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(dispatch_complete_callback, status));
}

// Dispatches the notificationclick on |service_worker_registration| if the
// registration was available. Must be called on the IO thread.
void DispatchNotificationClickEventOnRegistration(
    const NotificationDatabaseData& notification_database_data,
    int action_index,
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Trying to dispatch notification for SW with status: "
            << service_worker_status << " action_index: " << action_index;
#endif
  if (service_worker_status == SERVICE_WORKER_OK) {
    base::Callback<void(ServiceWorkerStatusCode)> dispatch_event_callback =
        base::Bind(&NotificationClickEventFinished, dispatch_complete_callback,
                   service_worker_registration);

    DCHECK(service_worker_registration->active_version());
    service_worker_registration->active_version()
        ->DispatchNotificationClickEvent(
            dispatch_event_callback, notification_database_data.notification_id,
            notification_database_data.notification_data, action_index);
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
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
    case SERVICE_WORKER_OK:
      NOTREACHED();
      break;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(dispatch_complete_callback, status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the IO thread.
void FindServiceWorkerRegistration(
    const GURL& origin,
    int action_index,
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    bool success,
    const NotificationDatabaseData& notification_database_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Lookup for ServiceWoker Registration: success:" << success
            << " action_index: " << action_index;
#endif
  if (!success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(dispatch_complete_callback,
                   PERSISTENT_NOTIFICATION_STATUS_DATABASE_ERROR));
    return;
  }

  service_worker_context->FindReadyRegistrationForId(
      notification_database_data.service_worker_registration_id, origin,
      base::Bind(&DispatchNotificationClickEventOnRegistration,
                 notification_database_data, action_index,
                 dispatch_complete_callback));
}

// Reads the data associated with the |persistent_notification_id| belonging to
// |origin| from the notification context.
void ReadNotificationDatabaseData(
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index,
    const NotificationClickDispatchCompleteCallback& dispatch_complete_callback,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<PlatformNotificationContextImpl> notification_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  notification_context->ReadNotificationData(
      persistent_notification_id, origin,
      base::Bind(&FindServiceWorkerRegistration, origin, action_index,
                 dispatch_complete_callback, service_worker_context));
}

}  // namespace

// static
NotificationEventDispatcher* NotificationEventDispatcher::GetInstance() {
  return NotificationEventDispatcherImpl::GetInstance();
}

NotificationEventDispatcherImpl*
NotificationEventDispatcherImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<NotificationEventDispatcherImpl>::get();
}

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() {}

NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() {}

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index,
    const NotificationClickDispatchCompleteCallback&
        dispatch_complete_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(persistent_notification_id, 0);
  DCHECK(origin.is_valid());

  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  scoped_refptr<PlatformNotificationContextImpl> notification_context =
      static_cast<PlatformNotificationContextImpl*>(
          partition->GetPlatformNotificationContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ReadNotificationDatabaseData, persistent_notification_id,
                 origin, action_index, dispatch_complete_callback,
                 service_worker_context, notification_context));
}

}  // namespace content
