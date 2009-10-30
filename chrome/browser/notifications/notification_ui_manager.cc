// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/renderer_host/site_instance.h"

// A class which represents a notification waiting to be shown.
class QueuedNotification {
 public:
  QueuedNotification(const Notification& notification, Profile* profile)
      : notification_(notification),
        profile_(profile) {
  }

  const Notification& notification() const { return notification_; }
  Profile* profile() const { return profile_; }

 private:
  // The notification to be shown.
  Notification notification_;

  // Non owned pointer to the user's profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(QueuedNotification);
};

NotificationUIManager::NotificationUIManager()
    : balloon_collection_(NULL) {
}

NotificationUIManager::~NotificationUIManager() {
  STLDeleteElements(&show_queue_);
}

// static
NotificationUIManager* NotificationUIManager::Create() {
  BalloonCollectionImpl* balloons = new BalloonCollectionImpl();
  NotificationUIManager* instance = new NotificationUIManager();
  instance->Initialize(balloons);
  balloons->set_space_change_listener(instance);
  return instance;
}

void NotificationUIManager::Add(const Notification& notification,
                                Profile* profile) {
  LOG(INFO) << "Added notification. URL: "
            << notification.content_url().spec().c_str();
  show_queue_.push_back(
      new QueuedNotification(notification, profile));
  CheckAndShowNotifications();
}

void NotificationUIManager::CheckAndShowNotifications() {
  // TODO(johnnyg): http://crbug.com/25061 - Check for user idle/presentation.
  ShowNotifications();
}

void NotificationUIManager::ShowNotifications() {
  while (!show_queue_.empty() && balloon_collection_->HasSpace()) {
    scoped_ptr<QueuedNotification> queued_notification(show_queue_.front());
    show_queue_.pop_front();
    balloon_collection_->Add(queued_notification->notification(),
                             queued_notification->profile());
  }
}

void NotificationUIManager::OnBalloonSpaceChanged() {
  CheckAndShowNotifications();
}
