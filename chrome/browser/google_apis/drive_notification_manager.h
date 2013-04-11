// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_

#include "base/observer_list.h"
#include "chrome/browser/google_apis/drive_notification_observer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace google_apis {

// Informs observers when they should check Google Drive for updates.
// Conditions under which updates should be searched:
// 1. XMPP invalidation is received from Google Drive.
//    TODO(calvinlo): Implement public syncer::InvalidationHandler interface.
// 2. Polling timer counts down.
//    TODO(calvinlo): Also add in backup timer.
class DriveNotificationManager
    : public ProfileKeyedService {
 public:
  explicit DriveNotificationManager(Profile* profile);
  virtual ~DriveNotificationManager();

  // ProfileKeyedService override.
  virtual void Shutdown() OVERRIDE;

  // TODO(calvinlo): OVERRIDES for syncer::InvalidationHandler go here.

  void AddObserver(DriveNotificationObserver* observer);
  void RemoveObserver(DriveNotificationObserver* observer);

 private:
  void NotifyObserversToUpdate();

  Profile* profile_;
  ObserverList<DriveNotificationObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DriveNotificationManager);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_H_
