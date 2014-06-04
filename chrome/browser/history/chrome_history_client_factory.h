// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_FACTORY_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

class ChromeHistoryClient;
class Profile;

// Singleton that owns all ChromeHistoryClients and associates them with
// Profiles.
class ChromeHistoryClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeHistoryClient* GetForProfile(Profile* profile);

  static ChromeHistoryClientFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeHistoryClientFactory>;

  ChromeHistoryClientFactory();
  virtual ~ChromeHistoryClientFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_FACTORY_H_
