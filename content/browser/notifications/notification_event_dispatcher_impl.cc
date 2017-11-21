// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/browser/notifications/notification_message_filter.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/platform_notification_data.h"

namespace content {
namespace {

using NotificationDispatchCompleteCallback =
    base::Callback<void(PersistentNotificationStatus)>;
using NotificationOperationCallback =
    base::Callback<void(const ServiceWorkerRegistration*,
                        const NotificationDatabaseData&)>;
using NotificationOperationCallbackWithContext =
    base::Callback<void(const scoped_refptr<PlatformNotificationContext>&,
                        const ServiceWorkerRegistration*,
                        const NotificationDatabaseData&)>;

// To be called when a notification event has finished executing. Will post a
// task to call |dispatch_complete_callback| on the UI thread.
void NotificationEventFinished(
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    PersistentNotificationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(dispatch_complete_callback, status));
}

// To be called when a notification event has finished with a
// ServiceWorkerStatusCode result. Will call NotificationEventFinished with a
// PersistentNotificationStatus derived from the service worker status.
void ServiceWorkerNotificationEventFinished(
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode service_worker_status) {
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "The notification event has finished: " << service_worker_status;
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
  NotificationEventFinished(dispatch_complete_callback, status);
}

// Dispatches the given notification action event on
// |service_worker_registration| if the registration was available. Must be
// called on the IO thread.
void DispatchNotificationEventOnRegistration(
    const NotificationDatabaseData& notification_database_data,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& dispatch_event_action,
    const NotificationDispatchCompleteCallback& dispatch_error_callback,
    ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Trying to dispatch notification for SW with status: "
            << service_worker_status;
#endif
  if (service_worker_status == SERVICE_WORKER_OK) {
    DCHECK(service_worker_registration->active_version());

    dispatch_event_action.Run(service_worker_registration.get(),
                              notification_database_data);
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
                          base::BindOnce(dispatch_error_callback, status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the IO thread.
void FindServiceWorkerRegistration(
    const GURL& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& notification_action_callback,
    const NotificationDispatchCompleteCallback& dispatch_error_callback,
    bool success,
    const NotificationDatabaseData& notification_database_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Lookup for ServiceWoker Registration: success: " << success;
#endif
  if (!success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(dispatch_error_callback,
                       PERSISTENT_NOTIFICATION_STATUS_DATABASE_ERROR));
    return;
  }

  service_worker_context->FindReadyRegistrationForId(
      notification_database_data.service_worker_registration_id, origin,
      base::Bind(&DispatchNotificationEventOnRegistration,
                 notification_database_data, notification_context,
                 notification_action_callback, dispatch_error_callback));
}

// Reads the data associated with the |notification_id| belonging to |origin|
// from the notification context.
void ReadNotificationDatabaseData(
    const std::string& notification_id,
    const GURL& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& notification_read_callback,
    const NotificationDispatchCompleteCallback& dispatch_error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  notification_context->ReadNotificationData(
      notification_id, origin,
      base::Bind(&FindServiceWorkerRegistration, origin, service_worker_context,
                 notification_context, notification_read_callback,
                 dispatch_error_callback));
}

// -----------------------------------------------------------------------------

// Dispatches the notificationclick event on |service_worker|. Must be called on
// the IO thread, and with the worker running.
void DispatchNotificationClickEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const ServiceWorkerVersion::LegacyStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK, callback);

  int action_index_int = -1 /* no value */;
  if (action_index.has_value())
    action_index_int = action_index.value();

  service_worker->event_dispatcher()->DispatchNotificationClickEvent(
      notification_database_data.notification_id,
      notification_database_data.notification_data, action_index_int, reply,
      service_worker->CreateSimpleEventCallback(request_id));
}

// Dispatches the notification click event on the |service_worker_registration|.
void DoDispatchNotificationClickEvent(
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data) {
  ServiceWorkerVersion::LegacyStatusCallback status_callback = base::Bind(
      &ServiceWorkerNotificationEventFinished, dispatch_complete_callback);
  service_worker_registration->active_version()->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK,
      base::BindOnce(
          &DispatchNotificationClickEventOnWorker,
          base::WrapRefCounted(service_worker_registration->active_version()),
          notification_database_data, action_index, reply, status_callback),
      status_callback);
}

// -----------------------------------------------------------------------------

// Called when the notification data has been deleted to finish the notification
// close event.
void OnPersistentNotificationDataDeleted(
    ServiceWorkerStatusCode service_worker_status,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    bool success) {
  if (service_worker_status != SERVICE_WORKER_OK) {
    ServiceWorkerNotificationEventFinished(dispatch_complete_callback,
                                           service_worker_status);
    return;
  }
  NotificationEventFinished(
      dispatch_complete_callback,
      success ? PERSISTENT_NOTIFICATION_STATUS_SUCCESS
              : PERSISTENT_NOTIFICATION_STATUS_DATABASE_ERROR);
}

// Called when the persistent notification close event has been handled
// to remove the notification from the database.
void DeleteNotificationDataFromDatabase(
    const std::string& notification_id,
    const GURL& origin,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode status_code) {
  notification_context->DeleteNotificationData(
      notification_id, origin,
      base::Bind(&OnPersistentNotificationDataDeleted, status_code,
                 dispatch_complete_callback));
}

// Dispatches the notificationclose event on |service_worker|. Must be called on
// the IO thread, and with the worker running.
void DispatchNotificationCloseEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    const ServiceWorkerVersion::LegacyStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE, callback);

  service_worker->event_dispatcher()->DispatchNotificationCloseEvent(
      notification_database_data.notification_id,
      notification_database_data.notification_data,
      service_worker->CreateSimpleEventCallback(request_id));
}

// Actually dispatches the notification close event on the service worker
// registration.
void DoDispatchNotificationCloseEvent(
    const std::string& notification_id,
    bool by_user,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data) {
  const ServiceWorkerVersion::LegacyStatusCallback dispatch_event_callback =
      base::Bind(&DeleteNotificationDataFromDatabase, notification_id,
                 notification_database_data.origin, notification_context,
                 dispatch_complete_callback);
  if (by_user) {
    service_worker_registration->active_version()->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE,
        base::BindOnce(
            &DispatchNotificationCloseEventOnWorker,
            base::WrapRefCounted(service_worker_registration->active_version()),
            notification_database_data, dispatch_event_callback),
        dispatch_event_callback);
  } else {
    dispatch_event_callback.Run(ServiceWorkerStatusCode::SERVICE_WORKER_OK);
  }
}

// Dispatches any notification event. The actual, specific event dispatch should
// be done by the |notification_action_callback|.
void DispatchNotificationEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const NotificationOperationCallbackWithContext&
        notification_action_callback,
    const NotificationDispatchCompleteCallback& notification_error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());

  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  scoped_refptr<PlatformNotificationContext> notification_context =
      partition->GetPlatformNotificationContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &ReadNotificationDatabaseData, notification_id, origin,
          service_worker_context, notification_context,
          base::Bind(notification_action_callback, notification_context),
          notification_error_callback));
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

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() = default;
NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() = default;

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  // TODO(peter): Remove AdaptCallbackForRepeating() when the dependencies of
  // the NotificationEventDispatcherImpl have updated to using OnceCallbacks.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(dispatch_complete_callback));

  DispatchNotificationEvent(
      browser_context, notification_id, origin,
      base::Bind(&DoDispatchNotificationClickEvent, action_index, reply,
                 repeating_callback),
      repeating_callback /* notification_error_callback */);
}

void NotificationEventDispatcherImpl::DispatchNotificationCloseEvent(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    bool by_user,
    NotificationDispatchCompleteCallback dispatch_complete_callback) {
  // TODO(peter): Remove AdaptCallbackForRepeating() when the dependencies of
  // the NotificationEventDispatcherImpl have updated to using OnceCallbacks.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(dispatch_complete_callback));

  DispatchNotificationEvent(
      browser_context, notification_id, origin,
      base::Bind(&DoDispatchNotificationCloseEvent, notification_id, by_user,
                 repeating_callback),
      repeating_callback /* notification_error_callback */);
}

void NotificationEventDispatcherImpl::RegisterNonPersistentNotification(
    const std::string& notification_id,
    int renderer_id,
    int non_persistent_id) {
  if (non_persistent_ids_.count(notification_id) &&
      non_persistent_ids_[notification_id] != non_persistent_id) {
    // Notify close for a previously displayed notification with the same id,
    // this can happen when replacing a non-persistent notification with the
    // same tag since from the JS point of view there will be two notification
    // objects and the old one needs to receive the close event.
    // TODO(miguelg) this is probably not the right layer to do this.
    DispatchNonPersistentCloseEvent(notification_id);
  }
  renderer_ids_[notification_id] = renderer_id;
  non_persistent_ids_[notification_id] = non_persistent_id;
}

void NotificationEventDispatcherImpl::DispatchNonPersistentShowEvent(
    const std::string& notification_id) {
  if (!renderer_ids_.count(notification_id))
    return;
  DCHECK(non_persistent_ids_.count(notification_id));

  RenderProcessHost* sender =
      RenderProcessHost::FromID(renderer_ids_[notification_id]);
  if (!sender)
    return;

  sender->Send(new PlatformNotificationMsg_DidShow(
      non_persistent_ids_[notification_id]));
}

void NotificationEventDispatcherImpl::DispatchNonPersistentClickEvent(
    const std::string& notification_id) {
  if (!renderer_ids_.count(notification_id))
    return;
  DCHECK(non_persistent_ids_.count(notification_id));

  RenderProcessHost* sender =
      RenderProcessHost::FromID(renderer_ids_[notification_id]);

  // This can happen when a notification is clicked by the user but the
  // renderer does not exist any more, for example because the tab has been
  // closed.
  if (!sender)
    return;
  sender->Send(new PlatformNotificationMsg_DidClick(
      non_persistent_ids_[notification_id]));
}

void NotificationEventDispatcherImpl::DispatchNonPersistentCloseEvent(
    const std::string& notification_id) {
  if (!renderer_ids_.count(notification_id))
    return;
  DCHECK(non_persistent_ids_.count(notification_id));

  RenderProcessHost* sender =
      RenderProcessHost::FromID(renderer_ids_[notification_id]);

  // This can happen when a notification is closed by the user but the
  // renderer does not exist any more, for example because the tab has been
  // closed.
  if (!sender)
    return;

  sender->Send(new PlatformNotificationMsg_DidClose(
      non_persistent_ids_[notification_id]));

  // No interaction will follow anymore once the notification has been closed.
  non_persistent_ids_.erase(notification_id);
  renderer_ids_.erase(notification_id);
}

void NotificationEventDispatcherImpl::RendererGone(int renderer_id) {
  for (auto iter = renderer_ids_.begin(); iter != renderer_ids_.end();) {
    if (iter->second == renderer_id) {
      non_persistent_ids_.erase(iter->first);
      iter = renderer_ids_.erase(iter);
    } else {
      iter++;
    }
  }
}

}  // namespace content
