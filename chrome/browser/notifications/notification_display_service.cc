// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/features/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#endif

namespace {

void OperationCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

}  // namespace

// static
NotificationDisplayService* NotificationDisplayService::GetForProfile(
    Profile* profile) {
  return NotificationDisplayServiceFactory::GetForProfile(profile);
}

#if !defined(OS_CHROMEOS)
// static
NotificationDisplayService*
NotificationDisplayService::GetForSystemNotifications() {
  return nullptr;
}
#endif

NotificationDisplayService::NotificationDisplayService(Profile* profile)
    : profile_(profile) {
  AddNotificationHandler(NotificationHandler::Type::WEB_NON_PERSISTENT,
                         std::make_unique<NonPersistentNotificationHandler>());
  AddNotificationHandler(NotificationHandler::Type::WEB_PERSISTENT,
                         std::make_unique<PersistentNotificationHandler>());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  AddNotificationHandler(
      NotificationHandler::Type::EXTENSION,
      std::make_unique<extensions::ExtensionNotificationHandler>());
#endif
}

NotificationDisplayService::~NotificationDisplayService() = default;

void NotificationDisplayService::AddNotificationHandler(
    NotificationHandler::Type notification_type,
    std::unique_ptr<NotificationHandler> handler) {
  DCHECK(handler);
  DCHECK_EQ(notification_handlers_.count(notification_type), 0u);
  notification_handlers_[notification_type] = std::move(handler);
}

void NotificationDisplayService::RemoveNotificationHandler(
    NotificationHandler::Type notification_type) {
  auto iter = notification_handlers_.find(notification_type);
  DCHECK(iter != notification_handlers_.end());
  notification_handlers_.erase(iter);
}

NotificationHandler* NotificationDisplayService::GetNotificationHandler(
    NotificationHandler::Type notification_type) {
  auto found = notification_handlers_.find(notification_type);
  if (found != notification_handlers_.end())
    return found->second.get();
  return nullptr;
}

void NotificationDisplayService::ProcessNotificationOperation(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  if (!handler) {
    LOG(ERROR) << "Unable to find a handler for "
               << static_cast<int>(notification_type);
    return;
  }

  // TODO(crbug.com/766854): Plumb this through from the notification platform
  // bridges so they can report completion of the operation as needed.
  base::OnceClosure completed_closure = base::BindOnce(&OperationCompleted);

  switch (operation) {
    case NotificationCommon::CLICK:
      handler->OnClick(profile_, origin, notification_id, action_index, reply,
                       std::move(completed_closure));
      break;
    case NotificationCommon::CLOSE:
      DCHECK(by_user.has_value());
      handler->OnClose(profile_, origin, notification_id, by_user.value(),
                       std::move(completed_closure));
      break;
    case NotificationCommon::DISABLE_PERMISSION:
      handler->DisableNotifications(profile_, origin);
      break;
    case NotificationCommon::SETTINGS:
      handler->OpenSettings(profile_, origin);
      break;
  }
}
