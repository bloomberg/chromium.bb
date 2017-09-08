// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service.h"

#include "base/memory/ptr_util.h"
#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "extensions/features/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#endif

NotificationDisplayService::NotificationDisplayService(Profile* profile)
    : profile_(profile) {
  AddNotificationHandler(NotificationCommon::NON_PERSISTENT,
                         base::MakeUnique<NonPersistentNotificationHandler>());
  AddNotificationHandler(NotificationCommon::PERSISTENT,
                         base::MakeUnique<PersistentNotificationHandler>());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  AddNotificationHandler(
      NotificationCommon::EXTENSION,
      base::MakeUnique<extensions::ExtensionNotificationHandler>());
#endif
}

NotificationDisplayService::~NotificationDisplayService() = default;

void NotificationDisplayService::AddNotificationHandler(
    NotificationCommon::Type notification_type,
    std::unique_ptr<NotificationHandler> handler) {
  DCHECK(handler);
  DCHECK_EQ(notification_handlers_.count(notification_type), 0u);
  notification_handlers_[notification_type] = std::move(handler);
}

NotificationHandler* NotificationDisplayService::GetNotificationHandler(
    NotificationCommon::Type notification_type) {
  DCHECK(notification_handlers_.find(notification_type) !=
         notification_handlers_.end())
      << notification_type << " is not registered.";
  return notification_handlers_[notification_type].get();
}

void NotificationDisplayService::ProcessNotificationOperation(
    NotificationCommon::Operation operation,
    NotificationCommon::Type notification_type,
    const std::string& origin,
    const std::string& notification_id,
    int action_index,
    const base::NullableString16& reply,
    bool by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  if (!handler) {
    LOG(ERROR) << "Unable to find a handler for " << notification_type;
    return;
  }
  switch (operation) {
    case NotificationCommon::CLICK:
      handler->OnClick(profile_, origin, notification_id, action_index, reply);
      break;
    case NotificationCommon::CLOSE:
      handler->OnClose(profile_, origin, notification_id, by_user);
      break;
    case NotificationCommon::SETTINGS:
      handler->OpenSettings(profile_);
      break;
  }
}

bool NotificationDisplayService::ShouldDisplayOverFullscreen(
    const GURL& origin,
    NotificationCommon::Type notification_type) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  return handler->ShouldDisplayOnFullScreen(profile_, origin.spec());
}
