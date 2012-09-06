// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SpellCheckHost;
class SpellCheckProfile;

// Entry into the SpellCheck system.
//
// Internally, this owns all SpellCheckProfile objects, but these aren't
// exposed to the callers. Instead, the SpellCheckProfile may or may not hand
// out SpellCheckHost objects for consumption outside the SpellCheck system.
class SpellCheckFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the spell check host. This may be NULL.
  static SpellCheckHost* GetHostForProfile(Profile* profile);

  // If |force| is false, and the spellchecker is already initialized (or is in
  // the process of initializing), then do nothing. Otherwise clobber the
  // current spellchecker and replace it with a new one.
  static void ReinitializeSpellCheckHost(Profile* profile, bool force);

  static SpellCheckFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SpellCheckFactory>;

  SpellCheckFactory();
  virtual ~SpellCheckFactory();

  // Fetches the internal object for |profile|.
  SpellCheckProfile* GetSpellCheckProfile(Profile* profile);

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckFactory);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_FACTORY_H_
