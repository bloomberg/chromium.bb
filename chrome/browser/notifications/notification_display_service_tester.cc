// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_tester.h"

#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/message_center/notification.h"

NotificationDisplayServiceTester::NotificationDisplayServiceTester(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);

  display_service_ = static_cast<StubNotificationDisplayService*>(
      NotificationDisplayServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_, &StubNotificationDisplayService::FactoryForTests));
}

NotificationDisplayServiceTester::~NotificationDisplayServiceTester() {
  NotificationDisplayServiceFactory::GetInstance()->SetTestingFactory(profile_,
                                                                      nullptr);
}

void NotificationDisplayServiceTester::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  display_service_->SetNotificationAddedClosure(std::move(closure));
}

std::vector<message_center::Notification>
NotificationDisplayServiceTester::GetDisplayedNotificationsForType(
    NotificationCommon::Type type) {
  return display_service_->GetDisplayedNotificationsForType(type);
}

const NotificationCommon::Metadata*
NotificationDisplayServiceTester::GetMetadataForNotification(
    const message_center::Notification& notification) {
  return display_service_->GetMetadataForNotification(notification);
}

void NotificationDisplayServiceTester::RemoveNotification(
    NotificationCommon::Type type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  display_service_->RemoveNotification(type, notification_id, by_user, silent);
}

void NotificationDisplayServiceTester::RemoveAllNotifications(
    NotificationCommon::Type type,
    bool by_user) {
  display_service_->RemoveAllNotifications(type, by_user);
}
