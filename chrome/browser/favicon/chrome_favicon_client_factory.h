// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_
#define CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_favicon_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

// Singleton that owns all ChromeFaviconClients and associates them with
// Profiles.
class ChromeFaviconClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of FaviconClient associated with this profile
  // (creating one if none exists).
  static FaviconClient* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static ChromeFaviconClientFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeFaviconClientFactory>;

  ChromeFaviconClientFactory();
  virtual ~ChromeFaviconClientFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_
