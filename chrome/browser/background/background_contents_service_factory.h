// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BackgroundContentsService;
class Profile;

// Singleton that owns all BackgroundContentsServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated BackgroundContentsService.
class BackgroundContentsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BackgroundContentsService* GetForProfile(Profile* profile);

  static BackgroundContentsServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BackgroundContentsServiceFactory>;

  BackgroundContentsServiceFactory();
  virtual ~BackgroundContentsServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
