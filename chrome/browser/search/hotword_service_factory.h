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
  // Returns the HotwordService for |context|.
  static HotwordService* GetForProfile(content::BrowserContext* context);

  static HotwordServiceFactory* GetInstance();

  // Returns true to show the opt in pop up for |context|.
  static bool ShouldShowOptInPopup(content::BrowserContext* context);

  // Returns true if the hotwording service is available for |context|.
  static bool IsServiceAvailable(content::BrowserContext* context);

  // Returns true if hotwording is allowed for |context|.
  static bool IsHotwordAllowed(content::BrowserContext* context);

  // Passes control to the individual service to deal with reloading the
  // hotword extension as necessary.
  static bool RetryHotwordExtension(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<HotwordServiceFactory>;

  HotwordServiceFactory();
  virtual ~HotwordServiceFactory();

  // Overrides from BrowserContextKeyedServiceFactory:
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(HotwordServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_FACTORY_H_
