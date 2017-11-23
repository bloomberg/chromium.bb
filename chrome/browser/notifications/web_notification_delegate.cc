// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_notification_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"

WebNotificationDelegate::WebNotificationDelegate(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const std::string& notification_id,
    const GURL& origin)
    : notification_type_(notification_type),
      profile_(profile),
      notification_id_(notification_id),
      origin_(origin) {}

WebNotificationDelegate::~WebNotificationDelegate() {}

void WebNotificationDelegate::SettingsClick() {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  NotificationCommon::OpenNotificationSettings(profile_);
#endif
}

void WebNotificationDelegate::DisableNotification() {
  DCHECK_NE(notification_type_, NotificationHandler::Type::EXTENSION);
  DesktopNotificationProfileUtil::DenyPermission(profile_, origin_);
}

void WebNotificationDelegate::Close(bool by_user) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLOSE, notification_type_, origin(), notification_id_,
      base::nullopt /* action_index */, base::nullopt /* reply */, by_user);
}

void WebNotificationDelegate::Click() {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin(), notification_id_,
      base::nullopt /* action_index */, base::nullopt /* reply */,
      base::nullopt /* by_user */);
}

void WebNotificationDelegate::ButtonClick(int action_index) {
  DCHECK_GE(action_index, 0);
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin(), notification_id_,
      action_index, base::nullopt /* reply */, base::nullopt /* by_user */);
}

void WebNotificationDelegate::ButtonClickWithReply(
    int action_index,
    const base::string16& reply) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin(), notification_id_,
      action_index, reply, base::nullopt /* by_user */);
}
