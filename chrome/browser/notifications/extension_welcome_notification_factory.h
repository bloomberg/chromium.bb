// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ExtensionWelcomeNotification;

// Singleton owning the extension welcome notification objects and associates
// them with the browser context for which they may have to be shown.
class ExtensionWelcomeNotificationFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ExtensionWelcomeNotification instance to be used for |context|.
  static ExtensionWelcomeNotification* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionWelcomeNotificationFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionWelcomeNotificationFactory>;

  ExtensionWelcomeNotificationFactory();
  virtual ~ExtensionWelcomeNotificationFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_FACTORY_H_
