// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_test_util.h"

MockNotificationDelegate::MockNotificationDelegate(const std::string& id)
    : id_(id) {}

MockNotificationDelegate::~MockNotificationDelegate() {}

std::string MockNotificationDelegate::id() const { return id_; }

// TODO(peter): |notification_| should be initialized with the correct origin.
StubNotificationUIManager::StubNotificationUIManager(const GURL& welcome_origin)
    : notification_(GURL(),
                    base::string16(),
                    base::string16(),
                    gfx::Image(),
                    base::string16(),
                    base::string16(),
                    new MockNotificationDelegate("stub")),
      profile_(NULL),
      welcome_origin_(welcome_origin),
      welcomed_(false),
      added_notifications_(0U) {
}

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

const Notification* StubNotificationUIManager::FindById(
    const std::string& delegate_id,
    ProfileID profile_id) const {
  if (notification_.delegate_id() == delegate_id && profile_ == profile_id)
    return &notification_;
  else
    return NULL;
}

bool StubNotificationUIManager::CancelById(const std::string& delegate_id,
                                           ProfileID profile_id) {
  dismissed_id_ = delegate_id;
  return true;
}

std::set<std::string>
StubNotificationUIManager::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  std::set<std::string> delegate_ids;
  if (source == notification_.origin_url() && profile->IsSameProfile(profile_))
    delegate_ids.insert(notification_.delegate_id());
  return delegate_ids;
}

bool StubNotificationUIManager::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  return false;
}

bool StubNotificationUIManager::CancelAllByProfile(ProfileID profile_id) {
  return false;
}

void StubNotificationUIManager::CancelAll() {}
