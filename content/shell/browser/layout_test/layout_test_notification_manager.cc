// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"

namespace content {
namespace {

// The Web Notification layout tests don't care about the lifetime of the
// Service Worker when a notificationclick event has been dispatched.
void OnEventDispatchComplete(PersistentNotificationStatus status) {}

}  // namespace

LayoutTestNotificationManager::LayoutTestNotificationManager()
    : weak_factory_(this) {}

LayoutTestNotificationManager::PersistentNotification::PersistentNotification()
    : browser_context(nullptr),
      service_worker_registration_id(0) {}

LayoutTestNotificationManager::~LayoutTestNotificationManager() {}

PermissionStatus LayoutTestNotificationManager::RequestPermission(
    const GURL& origin) {
  return GetPermissionStatus(origin);
}

blink::WebNotificationPermission
LayoutTestNotificationManager::CheckPermission(const GURL& origin) {
  base::AutoLock lock(permission_lock_);
  const auto& iter = permission_map_.find(origin);
  if (iter != permission_map_.end())
    return iter->second;

  return blink::WebNotificationPermissionDefault;
}

void LayoutTestNotificationManager::SetPermission(
    const GURL& origin,
    blink::WebNotificationPermission permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(permission_lock_);

  permission_map_[origin] = permission;
}

void LayoutTestNotificationManager::ClearPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(permission_lock_);

  permission_map_.clear();
}

void LayoutTestNotificationManager::DisplayNotification(
    BrowserContext* browser_context,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data,
    scoped_ptr<DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string title = base::UTF16ToUTF8(notification_data.title);

  DCHECK(cancel_callback);
  *cancel_callback = base::Bind(&LayoutTestNotificationManager::Close,
                                weak_factory_.GetWeakPtr(),
                                title);

  ReplaceNotificationIfNeeded(notification_data);

  page_notifications_[title] = delegate.release();
  page_notifications_[title]->NotificationDisplayed();
}

void LayoutTestNotificationManager::DisplayPersistentNotification(
    BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string title = base::UTF16ToUTF8(notification_data.title);

  ReplaceNotificationIfNeeded(notification_data);

  PersistentNotification notification;
  notification.browser_context = browser_context;
  notification.origin = origin;
  notification.notification_data = notification_data;
  notification.service_worker_registration_id = service_worker_registration_id;
  notification.persistent_id = base::GenerateGUID();

  persistent_notifications_[title] = notification;
}

void LayoutTestNotificationManager::ClosePersistentNotification(
    BrowserContext* browser_context,
    const std::string& persistent_notification_id) {
  for (const auto& iter : persistent_notifications_) {
    if (iter.second.persistent_id != persistent_notification_id)
      continue;

    persistent_notifications_.erase(iter.first);
    return;
  }
}

void LayoutTestNotificationManager::SimulateClick(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // First check for page-notifications with the given title.
  const auto& page_iterator = page_notifications_.find(title);
  if (page_iterator != page_notifications_.end()) {
    page_iterator->second->NotificationClick();
    return;
  }

  // Then check for persistent notifications with the given title.
  const auto& persistent_iterator = persistent_notifications_.find(title);
  if (persistent_iterator == persistent_notifications_.end())
    return;

  const PersistentNotification& notification = persistent_iterator->second;
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          notification.browser_context,
          notification.origin,
          notification.service_worker_registration_id,
          notification.persistent_id,
          notification.notification_data,
          base::Bind(&OnEventDispatchComplete));
}

PermissionStatus LayoutTestNotificationManager::GetPermissionStatus(
    const GURL& origin) {
  switch (CheckPermission(origin)) {
    case blink::WebNotificationPermissionAllowed:
      return PERMISSION_STATUS_GRANTED;
    case blink::WebNotificationPermissionDenied:
      return PERMISSION_STATUS_DENIED;
    case blink::WebNotificationPermissionDefault:
      return PERMISSION_STATUS_ASK;
  }

  NOTREACHED();
  return PERMISSION_STATUS_DENIED;
}

blink::WebNotificationPermission
LayoutTestNotificationManager::CheckPermissionOnUIThread(
    BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckPermission(origin);
}

blink::WebNotificationPermission
LayoutTestNotificationManager::CheckPermissionOnIOThread(
    ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return CheckPermission(origin);
}

void LayoutTestNotificationManager::Close(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iterator = page_notifications_.find(title);
  if (iterator == page_notifications_.end())
    return;

  iterator->second->NotificationClosed();
}

void LayoutTestNotificationManager::ReplaceNotificationIfNeeded(
    const PlatformNotificationData& notification_data) {
  if (!notification_data.tag.length())
    return;

  std::string tag = notification_data.tag;
  const auto& replace_iter = replacements_.find(tag);
  if (replace_iter != replacements_.end()) {
    const std::string& previous_title = replace_iter->second;

    const auto& page_notification_iter =
        page_notifications_.find(previous_title);
    if (page_notification_iter != page_notifications_.end()) {
      DesktopNotificationDelegate* previous_delegate =
          page_notification_iter->second;

      previous_delegate->NotificationClosed();

      page_notifications_.erase(page_notification_iter);
      delete previous_delegate;
    }

    const auto& persistent_notification_iter =
        persistent_notifications_.find(previous_title);
    if (persistent_notification_iter != persistent_notifications_.end())
      persistent_notifications_.erase(persistent_notification_iter);
  }

  replacements_[tag] = base::UTF16ToUTF8(notification_data.title);
}

}  // namespace content
