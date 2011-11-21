// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PrefService;
class Profile;
class TemplateURLService;

// Singleton that owns all TemplateURLService and associates them with
// Profiles.
class TemplateURLServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TemplateURLService* GetForProfile(Profile* profile);

  static TemplateURLServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TemplateURLServiceFactory>;

  TemplateURLServiceFactory();
  virtual ~TemplateURLServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;
  virtual void ProfileShutdown(Profile* profile) OVERRIDE;
  virtual void ProfileDestroyed(Profile* profile) OVERRIDE;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_H_
