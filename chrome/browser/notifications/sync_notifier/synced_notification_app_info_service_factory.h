// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace notifier {

class SyncedNotificationAppInfoService;

class SyncedNotificationAppInfoServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Return the SyncedNotificationAppInfoService for this profile.
  static SyncedNotificationAppInfoService* GetForProfile(
      Profile* profile,
      Profile::ServiceAccessType service_access_type);

  // Get the factory.
  static SyncedNotificationAppInfoServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SyncedNotificationAppInfoServiceFactory>;

  SyncedNotificationAppInfoServiceFactory();
  virtual ~SyncedNotificationAppInfoServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_FACTORY_H_
