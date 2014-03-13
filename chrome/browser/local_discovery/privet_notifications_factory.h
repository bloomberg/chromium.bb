// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_FACTORY_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace local_discovery {

class PrivetNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of PrivetNotificationServiceFactory.
  static PrivetNotificationServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PrivetNotificationServiceFactory>;

  PrivetNotificationServiceFactory();
  virtual ~PrivetNotificationServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_NOTIFICATIONS_FACTORY_H_
