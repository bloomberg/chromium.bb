// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PrefRegistrySyncable;
class SpellcheckService;

// Entry into the SpellCheck system.
//
// Internally, this owns all SpellcheckService objects.
class SpellcheckServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the spell check host. This may be NULL.
  static SpellcheckService* GetForProfile(Profile* profile);

  static SpellcheckServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SpellcheckServiceFactory>;

  SpellcheckServiceFactory();
  virtual ~SpellcheckServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefRegistrySyncable* registry) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceFactory);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
