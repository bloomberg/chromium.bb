// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ChromeToMobileService;
class Profile;

class ChromeToMobileServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Get the singleton ChromeToMobileServiceFactory instance.
  static ChromeToMobileServiceFactory* GetInstance();

  // Get |profile|'s ChromeToMobileService, creating one if needed.
  static ChromeToMobileService* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ChromeToMobileServiceFactory>;

  explicit ChromeToMobileServiceFactory();
  virtual ~ChromeToMobileServiceFactory();

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileServiceFactory);
};

#endif  // CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_FACTORY_H_
