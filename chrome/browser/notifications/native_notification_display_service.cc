// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/native_notification_display_service.h"

#include "base/memory/ptr_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "chrome/browser/profiles/profile.h"

namespace {

std::string GetProfileId(Profile* profile) {
#if defined(OS_WIN)
  std::string profile_id =
      base::WideToUTF8(profile->GetPath().BaseName().value());
#elif defined(OS_POSIX)
  std::string profile_id = profile->GetPath().BaseName().value();
#endif
  return profile_id;
}

}  // namespace

NativeNotificationDisplayService::NativeNotificationDisplayService(
    Profile* profile,
    NotificationPlatformBridge* notification_bridge)
    : profile_(profile), notification_bridge_(notification_bridge) {
  DCHECK(profile_);
  DCHECK(notification_bridge_);

  AddNotificationHandler(NotificationCommon::NON_PERSISTENT,
                         base::MakeUnique<NonPersistentNotificationHandler>());
  AddNotificationHandler(NotificationCommon::PERSISTENT,
                         base::MakeUnique<PersistentNotificationHandler>());
}

NativeNotificationDisplayService::~NativeNotificationDisplayService() {}

void NativeNotificationDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const Notification& notification) {
  notification_bridge_->Display(notification_type, notification_id,
                                GetProfileId(profile_),
                                profile_->IsOffTheRecord(), notification);
  notification.delegate()->Display();
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  handler->RegisterNotification(notification_id, notification.delegate());
}

void NativeNotificationDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  notification_bridge_->Close(GetProfileId(profile_), notification_id);

  // TODO(miguelg): Figure out something better here, passing an empty
  // origin works because only non persistent notifications care about
  // this method for JS generated close calls and they don't require
  // the origin.
  handler->OnClose(profile_, "", notification_id, false /* by user */);
}

bool NativeNotificationDisplayService::GetDisplayed(
    std::set<std::string>* notifications) const {
  return notification_bridge_->GetDisplayed(
      GetProfileId(profile_), profile_->IsOffTheRecord(), notifications);
}

void NativeNotificationDisplayService::ProcessNotificationOperation(
    NotificationCommon::Operation operation,
    NotificationCommon::Type notification_type,
    const std::string& origin,
    const std::string& notification_id,
    int action_index,
    const base::NullableString16& reply) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  CHECK(handler);
  switch (operation) {
    case NotificationCommon::CLICK:
      handler->OnClick(profile_, origin, notification_id, action_index, reply);
      break;
    case NotificationCommon::CLOSE:
      handler->OnClose(profile_, origin, notification_id, true /* by_user */);
      break;
    case NotificationCommon::SETTINGS:
      handler->OpenSettings(profile_);
      break;
  }
}

void NativeNotificationDisplayService::AddNotificationHandler(
    NotificationCommon::Type notification_type,
    std::unique_ptr<NotificationHandler> handler) {
  DCHECK(handler);
  DCHECK_EQ(notification_handlers_.count(notification_type), 0u);
  notification_handlers_[notification_type] = std::move(handler);
}

void NativeNotificationDisplayService::RemoveNotificationHandler(
    NotificationCommon::Type notification_type) {
  notification_handlers_.erase(notification_type);
}

NotificationHandler* NativeNotificationDisplayService::GetNotificationHandler(
    NotificationCommon::Type notification_type) {
  DCHECK(notification_handlers_.find(notification_type) !=
         notification_handlers_.end())
      << notification_type << " is not registered.";
  return notification_handlers_[notification_type].get();
}
