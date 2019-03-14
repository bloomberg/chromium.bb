// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_proxy.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"

namespace content {

PlatformNotificationServiceProxy::PlatformNotificationServiceProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    BrowserContext* browser_context)
    : service_worker_context_(service_worker_context),
      browser_context_(browser_context),
      notification_service_(
          GetContentClient()->browser()->GetPlatformNotificationService()),
      weak_ptr_factory_(this) {}

PlatformNotificationServiceProxy::~PlatformNotificationServiceProxy() = default;

void PlatformNotificationServiceProxy::DoDisplayNotification(
    const NotificationDatabaseData& data,
    const GURL& service_worker_scope,
    DisplayResultCallback callback) {
  if (notification_service_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            &PlatformNotificationService::DisplayPersistentNotification,
            base::Unretained(notification_service_), browser_context_,
            data.notification_id, service_worker_scope, data.origin,
            data.notification_data,
            data.notification_resources.value_or(
                blink::NotificationResources())));
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), /* success= */ true,
                     data.notification_id));
}

void PlatformNotificationServiceProxy::VerifyServiceWorkerScope(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      registration->scope().GetOrigin() == data.origin) {
    DoDisplayNotification(data, registration->scope(), std::move(callback));
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       /* notification_id= */ ""));
  }
}

void PlatformNotificationServiceProxy::DisplayNotification(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback) {
  if (!service_worker_context_) {
    DoDisplayNotification(data, GURL(), std::move(callback));
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindReadyRegistrationForId,
          service_worker_context_, data.service_worker_registration_id,
          data.origin,
          base::BindOnce(
              &PlatformNotificationServiceProxy::VerifyServiceWorkerScope,
              weak_ptr_factory_.GetWeakPtr(), data, std::move(callback))));
}

void PlatformNotificationServiceProxy::CloseNotification(
    const std::string& notification_id) {
  if (!notification_service_)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PlatformNotificationService::ClosePersistentNotification,
                     base::Unretained(notification_service_), browser_context_,
                     notification_id));
}

void PlatformNotificationServiceProxy::ScheduleTrigger(base::Time timestamp) {
  if (!notification_service_)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PlatformNotificationService::ScheduleTrigger,
                     base::Unretained(notification_service_), browser_context_,
                     timestamp));
}

base::Time PlatformNotificationServiceProxy::GetNextTrigger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!notification_service_)
    return base::Time::Max();
  return notification_service_->ReadNextTriggerTimestamp(browser_context_);
}

}  // namespace content
