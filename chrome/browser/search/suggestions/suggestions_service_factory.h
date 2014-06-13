// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace suggestions {

class SuggestionsService;

// Singleton that owns all SuggestionsServices and associates them with
// Profiles.
class SuggestionsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the SuggestionsService for |profile|.
  static suggestions::SuggestionsService* GetForProfile(Profile* profile);

  static SuggestionsServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SuggestionsServiceFactory>;

  SuggestionsServiceFactory();
  virtual ~SuggestionsServiceFactory();

  // Overrides from BrowserContextKeyedServiceFactory:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceFactory);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_SERVICE_FACTORY_H_
