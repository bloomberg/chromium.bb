// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include "base/logging.h"

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  return new NotificationUIManagerAndroid();
}

NotificationUIManagerAndroid::NotificationUIManagerAndroid() {
}

NotificationUIManagerAndroid::~NotificationUIManagerAndroid() {
}

void NotificationUIManagerAndroid::Add(const Notification& notification,
                                       Profile* profile) {
  // TODO(peter): Implement the NotificationUIManagerAndroid class.
  NOTIMPLEMENTED();
}

bool NotificationUIManagerAndroid::Update(const Notification& notification,
                                          Profile* profile) {
  return false;
}

const Notification* NotificationUIManagerAndroid::FindById(
    const std::string& notification_id) const {
  return 0;
}

bool NotificationUIManagerAndroid::CancelById(
    const std::string& notification_id) {
  return false;
}

std::set<std::string>
NotificationUIManagerAndroid::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  return std::set<std::string>();
}

bool NotificationUIManagerAndroid::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  return false;
}

bool NotificationUIManagerAndroid::CancelAllByProfile(Profile* profile) {
  return false;
}

void NotificationUIManagerAndroid::CancelAll() {
}
