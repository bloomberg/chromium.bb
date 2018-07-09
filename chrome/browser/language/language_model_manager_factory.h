// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_
#define CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace language {
class LanguageModelManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Manages the language model for each profile. The particular language model
// provided depends on feature flags.
class LanguageModelManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LanguageModelManagerFactory* GetInstance();
  static language::LanguageModelManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend struct base::DefaultSingletonTraits<LanguageModelManagerFactory>;

  LanguageModelManagerFactory();
  ~LanguageModelManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(LanguageModelManagerFactory);
};

#endif  // CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_
