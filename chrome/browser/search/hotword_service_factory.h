// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class BrowserContextKeyedService;
class HotwordService;
class Profile;

// Singleton that owns all HotwordServices and associates them with Profiles.
class HotwordServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the HotwordService for |profile|.
  static HotwordService* GetForProfile(Profile* profile);

  static HotwordServiceFactory* GetInstance();

  // Returns true to show the opt in pop up for that profile.
  static bool ShouldShowOptInPopup(Profile* profile);

  // Returns true if the hotwording service is available for |profile|.
  static bool IsServiceAvailable(Profile* profile);

  // Returns true if hotwording is allowed for |profile|.
  static bool IsHotwordAllowed(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<HotwordServiceFactory>;

  HotwordServiceFactory();
  virtual ~HotwordServiceFactory();

  // Overrides from BrowserContextKeyedServiceFactory:
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(HotwordServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_
