// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
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
    int render_process_id,
    const url::Origin& origin,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request)
    : notification_context_(notification_context),
      browser_context_(browser_context),
      resource_context_(resource_context),
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

  blink::mojom::PermissionStatus permission_status =
      Service()->CheckPermissionOnIOThread(resource_context_, origin_.GetURL(),
                                           render_process_id_);

  std::move(callback).Run(permission_status);
}

void BlinkNotificationServiceImpl::OnConnectionError() {
  notification_context_->RemoveService(this);
  // |this| has now been deleted.
}

void BlinkNotificationServiceImpl::DisplayNonPersistentNotification(
    const PlatformNotificationData& platform_notification_data,
    const NotificationResources& notification_resources,
    blink::mojom::NonPersistentNotificationListenerPtr event_listener_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!Service())
    return;

  // TODO(https://crbug.com/595685): Generate a GUID in the
  // NotificationIdGenerator instead.
  static int request_id = 0;
  request_id++;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(
              origin_.GetURL(), platform_notification_data.tag, request_id,
              render_process_id_);

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

}  // namespace content
