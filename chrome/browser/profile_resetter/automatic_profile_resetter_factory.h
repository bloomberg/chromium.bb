// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_FACTORY_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_FACTORY_H_

#include "base/basictypes.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

class PrefRegistrySimple;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class AutomaticProfileResetter;

class AutomaticProfileResetterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutomaticProfileResetter* GetForBrowserContext(
      content::BrowserContext* context);
  static AutomaticProfileResetterFactory* GetInstance();

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend struct DefaultSingletonTraits<AutomaticProfileResetterFactory>;

  AutomaticProfileResetterFactory();
  virtual ~AutomaticProfileResetterFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  // BrowserContextKeyedBaseFactory:
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterFactory);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_FACTORY_H_
