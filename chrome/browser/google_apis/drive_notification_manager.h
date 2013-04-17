// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_

#include "base/observer_list.h"
#include "chrome/browser/google_apis/drive_notification_observer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "sync/notifier/invalidation_handler.h"

class Profile;

namespace google_apis {

// Informs observers when they should check Google Drive for updates.
// Conditions under which updates should be searched:
// 1. XMPP invalidation is received from Google Drive.
//    TODO(calvinlo): Implement public syncer::InvalidationHandler interface.
// 2. Polling timer counts down.
//    TODO(calvinlo): Also add in backup timer.
class DriveNotificationManager
    : public ProfileKeyedService,
      public syncer::InvalidationHandler {
 public:
  explicit DriveNotificationManager(Profile* profile);
  virtual ~DriveNotificationManager();

  // ProfileKeyedService override.
  virtual void Shutdown() OVERRIDE;

  // syncer::InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  void AddObserver(DriveNotificationObserver* observer);
  void RemoveObserver(DriveNotificationObserver* observer);

 private:
  void NotifyObserversToUpdate();

  // XMPP notification related methods.
  void RegisterDriveNotifications();
  bool IsDriveNotificationSupported();
  void SetPushNotificationEnabled(syncer::InvalidatorState state);

  Profile* profile_;
  ObserverList<DriveNotificationObserver> observers_;

  // XMPP notification related variables.
  // True when Drive File Sync Service is registered for Drive notifications.
  bool push_notification_registered_;
  // True once the first drive notification is received with OK state.
  bool push_notification_enabled_;

  // TODO(calvinlo): Polling variables to go here.

  DISALLOW_COPY_AND_ASSIGN(DriveNotificationManager);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_
