// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

// Singleton that owns all LogoServices and associates them with Profiles.
class LogoServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static search_provider_logos::LogoService* GetForProfile(Profile* profile);

  static LogoServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LogoServiceFactory>;

  LogoServiceFactory();
  ~LogoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(LogoServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_PROVIDER_LOGOS_LOGO_SERVICE_FACTORY_H_
