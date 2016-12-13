// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

#include "base/guid.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

namespace content {
namespace {

// The Web Notification layout tests don't care about the lifetime of the
// Service Worker when a notificationclick event has been dispatched.
void OnEventDispatchComplete(PersistentNotificationStatus status) {}

}  // namespace

LayoutTestNotificationManager::LayoutTestNotificationManager()
    : weak_factory_(this) {}

LayoutTestNotificationManager::~LayoutTestNotificationManager() {}

void LayoutTestNotificationManager::DisplayNotification(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const NotificationResources& notification_resources,
    std::unique_ptr<DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(cancel_callback);

  *cancel_callback = base::Bind(&LayoutTestNotificationManager::Close,
                                weak_factory_.GetWeakPtr(), notification_id);

  ReplaceNotificationIfNeeded(notification_id);

  non_persistent_notifications_[notification_id] = std::move(delegate);
  non_persistent_notifications_[notification_id]->NotificationDisplayed();

  notification_id_map_[base::UTF16ToUTF8(notification_data.title)] =
      notification_id;
}

void LayoutTestNotificationManager::DisplayPersistentNotification(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ReplaceNotificationIfNeeded(notification_id);

  PersistentNotification notification;
  notification.browser_context = browser_context;
  notification.origin = origin;

  persistent_notifications_[notification_id] = notification;

  notification_id_map_[base::UTF16ToUTF8(notification_data.title)] =
      notification_id;
}

void LayoutTestNotificationManager::ClosePersistentNotification(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  persistent_notifications_.erase(notification_id);
}

bool LayoutTestNotificationManager::GetDisplayedNotifications(
    BrowserContext* browser_context,
    std::set<std::string>* displayed_notifications) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(displayed_notifications);

  // Notifications will never outlive the lifetime of running layout tests.
  return false;
}

void LayoutTestNotificationManager::SimulateClick(
    const std::string& title,
    int action_index,
    const base::NullableString16& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto notification_id_iter = notification_id_map_.find(title);
  if (notification_id_iter == notification_id_map_.end())
    return;

  const std::string& notification_id = notification_id_iter->second;

  const auto persistent_iter = persistent_notifications_.find(notification_id);
  const auto non_persistent_iter =
      non_persistent_notifications_.find(notification_id);

  if (persistent_iter != persistent_notifications_.end()) {
    DCHECK(non_persistent_iter == non_persistent_notifications_.end());

    const PersistentNotification& notification = persistent_iter->second;
    content::NotificationEventDispatcher::GetInstance()
        ->DispatchNotificationClickEvent(
            notification.browser_context, notification_id, notification.origin,
            action_index, reply, base::Bind(&OnEventDispatchComplete));
  } else if (non_persistent_iter != non_persistent_notifications_.end()) {
    DCHECK_EQ(action_index, -1) << "Action buttons are only supported for "
                                   "persistent notifications";

    non_persistent_iter->second->NotificationClick();
  }
}

void LayoutTestNotificationManager::SimulateClose(const std::string& title,
                                                  bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto notification_id_iter = notification_id_map_.find(title);
  if (notification_id_iter == notification_id_map_.end())
    return;

  const std::string& notification_id = notification_id_iter->second;

  const auto& persistent_iter = persistent_notifications_.find(notification_id);
  if (persistent_iter == persistent_notifications_.end())
    return;

  const PersistentNotification& notification = persistent_iter->second;
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          notification.browser_context, notification_id, notification.origin,
          by_user, base::Bind(&OnEventDispatchComplete));
}

blink::mojom::PermissionStatus
LayoutTestNotificationManager::CheckPermissionOnUIThread(
    BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckPermission(origin);
}

blink::mojom::PermissionStatus
LayoutTestNotificationManager::CheckPermissionOnIOThread(
    ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return CheckPermission(origin);
}

void LayoutTestNotificationManager::Close(const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iterator = non_persistent_notifications_.find(notification_id);
  if (iterator == non_persistent_notifications_.end())
    return;

  iterator->second->NotificationClosed();
}

void LayoutTestNotificationManager::ReplaceNotificationIfNeeded(
    const std::string& notification_id) {
  const auto persistent_iter = persistent_notifications_.find(notification_id);
  const auto non_persistent_iter =
      non_persistent_notifications_.find(notification_id);

  if (persistent_iter != persistent_notifications_.end()) {
    DCHECK(non_persistent_iter == non_persistent_notifications_.end());
    persistent_notifications_.erase(persistent_iter);
  } else if (non_persistent_iter != non_persistent_notifications_.end()) {
    non_persistent_iter->second->NotificationClosed();
    non_persistent_notifications_.erase(non_persistent_iter);
  }
}

blink::mojom::PermissionStatus
LayoutTestNotificationManager::CheckPermission(const GURL& origin) {
  return LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->GetPermissionStatus(PermissionType::NOTIFICATIONS,
                            origin,
                            origin);
}

}  // namespace content
