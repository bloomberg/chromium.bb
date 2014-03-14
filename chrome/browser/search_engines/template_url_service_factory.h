// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class TemplateURLService;

// Singleton that owns all TemplateURLService and associates them with
// Profiles.
class TemplateURLServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TemplateURLService* GetForProfile(Profile* profile);

  static TemplateURLServiceFactory* GetInstance();

  static KeyedService* BuildInstanceFor(content::BrowserContext* profile);

 private:
  friend struct DefaultSingletonTraits<TemplateURLServiceFactory>;

  TemplateURLServiceFactory();
  virtual ~TemplateURLServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
