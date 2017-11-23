// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/native_notification_display_service.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

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
    : NotificationDisplayService(profile),
      profile_(profile),
      notification_bridge_(notification_bridge),
      notification_bridge_ready_(false),
      weak_factory_(this) {
  DCHECK(profile_);
  DCHECK(notification_bridge_);

  notification_bridge->SetReadyCallback(base::BindOnce(
      &NativeNotificationDisplayService::OnNotificationPlatformBridgeReady,
      weak_factory_.GetWeakPtr()));
}

NativeNotificationDisplayService::~NativeNotificationDisplayService() = default;

void NativeNotificationDisplayService::OnNotificationPlatformBridgeReady(
    bool success) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.UsingNativeNotificationCenter", success);
  if (success)
    notification_bridge_ready_ = true;

  // TODO(estade): this shouldn't be necessary in the succesful case, but some
  // notification bridges can't handle TRANSIENT notifications and still have to
  // fall back to the MessageCenter.
  message_center_display_service_ =
      std::make_unique<MessageCenterDisplayService>(profile_);

  while (!actions_.empty()) {
    std::move(actions_.front()).Run();
    actions_.pop();
  }
}

void NativeNotificationDisplayService::Display(
    NotificationHandler::Type notification_type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // TODO(estade): in the future, the reverse should also be true: a
  // non-TRANSIENT type implies no delegate.
  if (notification_type == NotificationHandler::Type::TRANSIENT)
    DCHECK(notification.delegate());

  if (ShouldUsePlatformBridge(notification_type)) {
    notification_bridge_->Display(notification_type, GetProfileId(profile_),
                                  profile_->IsOffTheRecord(), notification,
                                  std::move(metadata));
    NotificationHandler* handler = GetNotificationHandler(notification_type);
    if (handler)
      handler->OnShow(profile_, notification.id());
  } else if (message_center_display_service_) {
    message_center_display_service_->Display(notification_type, notification,
                                             std::move(metadata));
  } else {
    actions_.push(base::BindOnce(&NativeNotificationDisplayService::Display,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification, std::move(metadata)));
  }
}

void NativeNotificationDisplayService::Close(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  if (ShouldUsePlatformBridge(notification_type)) {
    notification_bridge_->Close(GetProfileId(profile_), notification_id);
  } else if (message_center_display_service_) {
    message_center_display_service_->Close(notification_type, notification_id);
  } else {
    actions_.push(base::BindOnce(&NativeNotificationDisplayService::Close,
                                 weak_factory_.GetWeakPtr(), notification_type,
                                 notification_id));
  }
}

void NativeNotificationDisplayService::GetDisplayed(
    const DisplayedNotificationsCallback& callback) {
  if (notification_bridge_ready_) {
    return notification_bridge_->GetDisplayed(
        GetProfileId(profile_), profile_->IsOffTheRecord(), callback);
  } else if (message_center_display_service_) {
    message_center_display_service_->GetDisplayed(callback);
  } else {
    actions_.push(
        base::BindOnce(&NativeNotificationDisplayService::GetDisplayed,
                       weak_factory_.GetWeakPtr(), callback));
  }
}

bool NativeNotificationDisplayService::ShouldUsePlatformBridge(
    NotificationHandler::Type notification_type) {
  return notification_bridge_ready_ &&
         NotificationPlatformBridge::CanHandleType(notification_type);
}
