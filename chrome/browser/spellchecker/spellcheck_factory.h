// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SpellcheckService;

// Entry into the SpellCheck system.
//
// Internally, this owns all SpellcheckService objects.
class SpellcheckServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the spell check host. This will create the SpellcheckService
  // if it does not already exist. This can return NULL.
  static SpellcheckService* GetForContext(content::BrowserContext* context);

  static SpellcheckService* GetForRenderProcessId(int render_process_id);

  static SpellcheckServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SpellcheckServiceFactory>;

  SpellcheckServiceFactory();
  virtual ~SpellcheckServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT);

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceFactory);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
