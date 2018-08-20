// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif

using content::BrowserThread;

PersistentNotificationHandler::PersistentNotificationHandler() = default;
PersistentNotificationHandler::~PersistentNotificationHandler() = default;

void PersistentNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(origin.is_valid());

  // TODO(peter): Should we do permission checks prior to forwarding to the
  // NotificationEventDispatcher?

  // If we programatically closed this notification, don't dispatch any event.
  if (PlatformNotificationServiceImpl::GetInstance()->WasClosedProgrammatically(
          notification_id)) {
    std::move(completed_closure).Run();
    return;
  }

  NotificationMetricsLogger* metrics_logger =
      NotificationMetricsLoggerFactory::GetForBrowserContext(profile);
  if (by_user)
    metrics_logger->LogPersistentNotificationClosedByUser();
  else
    metrics_logger->LogPersistentNotificationClosedProgrammatically();

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          profile, notification_id, origin, by_user,
          base::BindOnce(&PersistentNotificationHandler::OnCloseCompleted,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(completed_closure)));
}

void PersistentNotificationHandler::OnCloseCompleted(
    base::OnceClosure completed_closure,
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationCloseResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);

  std::move(completed_closure).Run();
}

void PersistentNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(origin.is_valid());

  NotificationMetricsLogger* metrics_logger =
      NotificationMetricsLoggerFactory::GetForBrowserContext(profile);

  blink::mojom::PermissionStatus permission_status =
      content::BrowserContext::GetPermissionController(profile)
          ->GetPermissionStatus(content::PermissionType::NOTIFICATIONS, origin,
                                origin);

  // Don't process click events when the |origin| doesn't have permission. This
  // can't be a DCHECK because of potential races with native notifications.
  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    metrics_logger->LogPersistentNotificationClickWithoutPermission();

    std::move(completed_closure).Run();
    return;
  }

  if (action_index.has_value())
    metrics_logger->LogPersistentNotificationActionButtonClick();
  else
    metrics_logger->LogPersistentNotificationClick();

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Ensure the browser stays alive while the event is processed. The keep alive
  // will be reset when all click events have been acknowledged.
  if (pending_click_dispatch_events_++ == 0) {
    click_dispatch_keep_alive_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
                            KeepAliveRestartOption::DISABLED));
  }
#endif

  // Notification clicks are considered a form of engagement with the |origin|,
  // thus we log the interaction with the Site Engagement service.
  SiteEngagementService::Get(profile)->HandleNotificationInteraction(origin);

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          profile, notification_id, origin, action_index, reply,
          base::BindOnce(&PersistentNotificationHandler::OnClickCompleted,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(completed_closure)));
}

void PersistentNotificationHandler::OnClickCompleted(
    base::OnceClosure completed_closure,
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationClickResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  DCHECK_GT(pending_click_dispatch_events_, 0);

  // Reset the keep alive if all in-flight events have been processed.
  if (--pending_click_dispatch_events_ == 0)
    click_dispatch_keep_alive_.reset();
#endif

  std::move(completed_closure).Run();
}

void PersistentNotificationHandler::DisableNotifications(Profile* profile,
                                                         const GURL& origin) {
  NotificationPermissionContext::UpdatePermission(profile, origin,
                                                  CONTENT_SETTING_BLOCK);
}

void PersistentNotificationHandler::OpenSettings(Profile* profile,
                                                 const GURL& origin) {
  NotificationCommon::OpenNotificationSettings(profile, origin);
}
