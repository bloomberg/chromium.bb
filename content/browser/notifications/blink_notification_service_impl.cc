// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Returns the implementation of the PlatformNotificationService. May be NULL.
PlatformNotificationService* Service() {
  return GetContentClient()->browser()->GetPlatformNotificationService();
}

}  // namespace

BlinkNotificationServiceImpl::BlinkNotificationServiceImpl(
    PlatformNotificationContextImpl* notification_context,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int render_process_id,
    const url::Origin& origin,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request)
    : notification_context_(notification_context),
      browser_context_(browser_context),
      resource_context_(resource_context),
      service_worker_context_(std::move(service_worker_context)),
      render_process_id_(render_process_id),
      origin_(origin),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(notification_context_);
  DCHECK(browser_context_);
  DCHECK(resource_context_);

  binding_.set_connection_error_handler(base::BindOnce(
      &BlinkNotificationServiceImpl::OnConnectionError,
      base::Unretained(this) /* the channel is owned by |this| */));
}

BlinkNotificationServiceImpl::~BlinkNotificationServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BlinkNotificationServiceImpl::GetPermissionStatus(
    GetPermissionStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!Service()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  blink::mojom::PermissionStatus permission_status = CheckPermissionStatus();

  std::move(callback).Run(permission_status);
}

void BlinkNotificationServiceImpl::OnConnectionError() {
  notification_context_->RemoveService(this);
  // |this| has now been deleted.
}

void BlinkNotificationServiceImpl::DisplayNonPersistentNotification(
    const std::string& token,
    const PlatformNotificationData& platform_notification_data,
    const NotificationResources& notification_resources,
    blink::mojom::NonPersistentNotificationListenerPtr event_listener_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!Service())
    return;
  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(origin_, token);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BlinkNotificationServiceImpl::
                         DisplayNonPersistentNotificationOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), notification_id,
                     origin_.GetURL(), platform_notification_data,
                     notification_resources,
                     event_listener_ptr.PassInterface()));
}

void BlinkNotificationServiceImpl::DisplayNonPersistentNotificationOnUIThread(
    const std::string& notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    blink::mojom::NonPersistentNotificationListenerPtrInfo listener_ptr_info) {
  NotificationEventDispatcherImpl* event_dispatcher =
      NotificationEventDispatcherImpl::GetInstance();
  event_dispatcher->RegisterNonPersistentNotificationListener(
      notification_id, std::move(listener_ptr_info));

  Service()->DisplayNotification(browser_context_, notification_id, origin,
                                 notification_data, notification_resources);
}

void BlinkNotificationServiceImpl::CloseNonPersistentNotification(
    const std::string& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!Service())
    return;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(origin_, token);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BlinkNotificationServiceImpl::
                         CloseNonPersistentNotificationOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), notification_id));
}

void BlinkNotificationServiceImpl::CloseNonPersistentNotificationOnUIThread(
    const std::string& notification_id) {
  Service()->CloseNotification(browser_context_, notification_id);

  NotificationEventDispatcherImpl::GetInstance()
      ->DispatchNonPersistentCloseEvent(notification_id);
}

blink::mojom::PermissionStatus
BlinkNotificationServiceImpl::CheckPermissionStatus() {
  return Service()->CheckPermissionOnIOThread(
      resource_context_, origin_.GetURL(), render_process_id_);
}

void BlinkNotificationServiceImpl::DisplayPersistentNotification(
    int64_t service_worker_registration_id,
    const PlatformNotificationData& platform_notification_data,
    const NotificationResources& notification_resources,
    DisplayPersistentNotificationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!Service()) {
    std::move(callback).Run(
        blink::mojom::PersistentNotificationError::INTERNAL_ERROR);
    return;
  }
  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        blink::mojom::PersistentNotificationError::PERMISSION_DENIED);
    return;
  }

  // TODO(awdf): Necessary to validate resources here?

  NotificationDatabaseData database_data;
  database_data.origin = origin_.GetURL();
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = platform_notification_data;

  notification_context_->WriteNotificationData(
      origin_.GetURL(), database_data,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &BlinkNotificationServiceImpl::DisplayPersistentNotificationWithId,
          weak_ptr_factory_.GetWeakPtr(), service_worker_registration_id,
          platform_notification_data, notification_resources,
          std::move(callback))));
}

void BlinkNotificationServiceImpl::DisplayPersistentNotificationWithId(
    int64_t service_worker_registration_id,
    const PlatformNotificationData& platform_notification_data,
    const NotificationResources& notification_resources,
    DisplayPersistentNotificationCallback callback,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!success) {
    std::move(callback).Run(
        blink::mojom::PersistentNotificationError::INTERNAL_ERROR);
    return;
  }

  service_worker_context_->FindReadyRegistrationForId(
      service_worker_registration_id, origin_.GetURL(),
      base::BindOnce(&BlinkNotificationServiceImpl::
                         DisplayPersistentNotificationWithIdForServiceWorker,
                     weak_ptr_factory_.GetWeakPtr(), notification_id,
                     platform_notification_data, notification_resources,
                     std::move(callback)));
}

void BlinkNotificationServiceImpl::
    DisplayPersistentNotificationWithIdForServiceWorker(
        const std::string& notification_id,
        const PlatformNotificationData& platform_notification_data,
        const NotificationResources& notification_resources,
        DisplayPersistentNotificationCallback callback,
        content::ServiceWorkerStatusCode service_worker_status,
        scoped_refptr<content::ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK) {
    std::move(callback).Run(
        blink::mojom::PersistentNotificationError::INTERNAL_ERROR);
    LOG(ERROR) << "Registration not found for " << origin_.GetURL().spec();
    // TODO(peter): Add UMA to track how often this occurs.
    return;
  }

  if (registration->pattern().GetOrigin() != origin_.GetURL()) {
    // Bail out, something's wrong.
    std::move(callback).Run(
        blink::mojom::PersistentNotificationError::INTERNAL_ERROR);
    return;
  }

  // Using base::Unretained here is safe because Service() returns a singleton.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &PlatformNotificationService::DisplayPersistentNotification,
          base::Unretained(Service()), browser_context_, notification_id,
          registration->pattern(), origin_.GetURL(), platform_notification_data,
          notification_resources));

  std::move(callback).Run(blink::mojom::PersistentNotificationError::NONE);
}

}  // namespace content
