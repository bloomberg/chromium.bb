// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_notification_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"


WebNotificationDelegate::WebNotificationDelegate(
    NotificationCommon::Type notification_type,
    Profile* profile,
    const std::string& notification_id,
    const GURL& origin)
    : notification_type_(notification_type),
      profile_(profile),
      notification_id_(notification_id),
      origin_(origin) {}

WebNotificationDelegate::~WebNotificationDelegate() {}

std::string WebNotificationDelegate::id() const {
  return notification_id_;
}

bool WebNotificationDelegate::SettingsClick() {
#if !defined(OS_CHROMEOS)
  NotificationCommon::OpenNotificationSettings(profile_);
  return true;
#else
  return false;
#endif
}

bool WebNotificationDelegate::ShouldDisplaySettingsButton() {
  return notification_type_ != NotificationCommon::EXTENSION;
}

bool WebNotificationDelegate::ShouldDisplayOverFullscreen() const {
  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);

  return display_service->ShouldDisplayOverFullscreen(origin_,
                                                      notification_type_);
}

void WebNotificationDelegate::Close(bool by_user) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLOSE, notification_type_, origin().spec(),
      notification_id_, base::nullopt /* action_index */,
      base::nullopt /* reply */, by_user);
}

void WebNotificationDelegate::Click() {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, base::nullopt /* action_index */,
      base::nullopt /* reply */, base::nullopt /* by_user */);
}

void WebNotificationDelegate::ButtonClick(int action_index) {
  DCHECK_GE(action_index, 0);
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, action_index, base::nullopt /* reply */,
      base::nullopt /* by_user */);
}

void WebNotificationDelegate::ButtonClickWithReply(
    int action_index,
    const base::string16& reply) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, action_index, reply, base::nullopt /* by_user */);
}
