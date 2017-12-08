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

namespace {
// Pointer to currently active tester, which is assumed to be a singleton.
NotificationDisplayServiceTester* g_tester = nullptr;
}  // namespace

NotificationDisplayServiceTester::NotificationDisplayServiceTester(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);

  display_service_ = static_cast<StubNotificationDisplayService*>(
      NotificationDisplayServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_, &StubNotificationDisplayService::FactoryForTests));
  g_tester = this;
}

NotificationDisplayServiceTester::~NotificationDisplayServiceTester() {
  g_tester = nullptr;
  NotificationDisplayServiceFactory::GetInstance()->SetTestingFactory(profile_,
                                                                      nullptr);
}

// static
NotificationDisplayServiceTester* NotificationDisplayServiceTester::Get() {
  return g_tester;
}

void NotificationDisplayServiceTester::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  display_service_->SetNotificationAddedClosure(std::move(closure));
}

std::vector<message_center::Notification>
NotificationDisplayServiceTester::GetDisplayedNotificationsForType(
    NotificationHandler::Type type) {
  return display_service_->GetDisplayedNotificationsForType(type);
}

base::Optional<message_center::Notification>
NotificationDisplayServiceTester::GetNotification(
    const std::string& notification_id) const {
  return display_service_->GetNotification(notification_id);
}

const NotificationCommon::Metadata*
NotificationDisplayServiceTester::GetMetadataForNotification(
    const message_center::Notification& notification) {
  return display_service_->GetMetadataForNotification(notification);
}

void NotificationDisplayServiceTester::SimulateClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id,
    base::Optional<int> action_index,
    base::Optional<base::string16> reply) {
  display_service_->SimulateClick(notification_type, notification_id,
                                  std::move(action_index), std::move(reply));
}

void NotificationDisplayServiceTester::SimulateSettingsClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  display_service_->SimulateSettingsClick(notification_type, notification_id);
}

void NotificationDisplayServiceTester::RemoveNotification(
    NotificationHandler::Type type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  display_service_->RemoveNotification(type, notification_id, by_user, silent);
}

void NotificationDisplayServiceTester::RemoveAllNotifications(
    NotificationHandler::Type type,
    bool by_user) {
  display_service_->RemoveAllNotifications(type, by_user);
}
