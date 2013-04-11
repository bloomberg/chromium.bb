// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_notification_manager.h"

#include "chrome/browser/google_apis/drive_notification_observer.h"
#include "chrome/browser/profiles/profile.h"

namespace google_apis {

// TODO(calvinlo): Constants for polling and XMPP sync mechanisms go here.

DriveNotificationManager::DriveNotificationManager(Profile* profile)
    : profile_(profile) {
  // TODO(calvinlo): Initialize member variables for polling and XMPP here.
}

DriveNotificationManager::~DriveNotificationManager() {}

void DriveNotificationManager::Shutdown() {
  // TODO(calvinlo): Unregister for Drive notifications goes here.
}

// TODO(calvinlo): Implementations for syncer::InvalidationHandler go here.
// Implement OnInvalidatorStateChange() and OnIncomingInvalidation().

void DriveNotificationManager::AddObserver(
    DriveNotificationObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveNotificationManager::RemoveObserver(
    DriveNotificationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveNotificationManager::NotifyObserversToUpdate() {
  FOR_EACH_OBSERVER(DriveNotificationObserver, observers_, CheckForUpdates());
}

}  // namespace google_apis
