// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace google_apis {

class DriveNotificationManager;

// Singleton that owns all DriveNotificationManager and associates them with
// profiles.
class DriveNotificationManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static DriveNotificationManager* GetForProfile(Profile* profile);

  static DriveNotificationManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<DriveNotificationManagerFactory>;

  DriveNotificationManagerFactory();
  virtual ~DriveNotificationManagerFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_
