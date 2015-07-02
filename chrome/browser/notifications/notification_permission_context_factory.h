// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class NotificationPermissionContext;
class Profile;

class NotificationPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NotificationPermissionContext* GetForProfile(Profile* profile);
  static NotificationPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NotificationPermissionContextFactory>;

  NotificationPermissionContextFactory();
  ~NotificationPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionContextFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_CONTEXT_FACTORY_H_
