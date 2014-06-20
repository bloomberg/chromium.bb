// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_test_util.h"

MockNotificationDelegate::MockNotificationDelegate(const std::string& id)
    : id_(id) {}

MockNotificationDelegate::~MockNotificationDelegate() {}

std::string MockNotificationDelegate::id() const { return id_; }

content::WebContents* MockNotificationDelegate::GetWebContents() const {
  return NULL;
}

StubNotificationUIManager::StubNotificationUIManager(const GURL& welcome_origin)
    : notification_(GURL(),
                    GURL(),
                    base::string16(),
                    base::string16(),
                    blink::WebTextDirectionDefault,
                    base::string16(),
                    base::string16(),
                    new MockNotificationDelegate("stub")),
      welcome_origin_(welcome_origin),
      welcomed_(false),
      added_notifications_(0U) {}

StubNotificationUIManager::~StubNotificationUIManager() {}

void StubNotificationUIManager::Add(const Notification& notification,
                                    Profile* profile) {
  // Make a deep copy of the notification that we can inspect.
  notification_ = notification;
  profile_ = profile;
  ++added_notifications_;

  if (notification.origin_url() == welcome_origin_)
    welcomed_ = true;
}

bool StubNotificationUIManager::Update(const Notification& notification,
                                       Profile* profile) {
  // Make a deep copy of the notification that we can inspect.
  notification_ = notification;
  profile_ = profile;
  return true;
}

const Notification* StubNotificationUIManager::FindById(const std::string& id)
    const {
  return (notification_.id() == id) ? &notification_ : NULL;
}

bool StubNotificationUIManager::CancelById(const std::string& notification_id) {
  dismissed_id_ = notification_id;
  return true;
}

std::set<std::string>
StubNotificationUIManager::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  std::set<std::string> notification_ids;
  if (source == notification_.origin_url() && profile->IsSameProfile(profile_))
    notification_ids.insert(notification_.delegate_id());
  return notification_ids;
}

bool StubNotificationUIManager::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  return false;
}

bool StubNotificationUIManager::CancelAllByProfile(Profile* profile) {
  return false;
}

void StubNotificationUIManager::CancelAll() {}
