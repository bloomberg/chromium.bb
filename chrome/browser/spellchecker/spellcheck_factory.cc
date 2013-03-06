// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "grit/locale_settings.h"

// static
SpellcheckService* SpellcheckServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SpellcheckService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SpellcheckServiceFactory* SpellcheckServiceFactory::GetInstance() {
  return Singleton<SpellcheckServiceFactory>::get();
}

SpellcheckServiceFactory::SpellcheckServiceFactory()
    : ProfileKeyedServiceFactory("SpellcheckService",
                                 ProfileDependencyManager::GetInstance()) {
  // TODO(erg): Uncomment these as they are initialized.
  // DependsOn(RequestContextFactory::GetInstance());
}

SpellcheckServiceFactory::~SpellcheckServiceFactory() {}

ProfileKeyedService* SpellcheckServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  // Many variables are initialized from the profile in the SpellcheckService.
  DCHECK(profile);
  SpellcheckService* spellcheck = new SpellcheckService(profile);

  // Instantiates Metrics object for spellchecking for use.
  spellcheck->StartRecordingMetrics(
      profile->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck));

  return spellcheck;
}

void SpellcheckServiceFactory::RegisterUserPrefs(
    PrefRegistrySyncable* user_prefs) {
  // TODO(estade): IDS_SPELLCHECK_DICTIONARY should be an ASCII string.
  user_prefs->RegisterLocalizedStringPref(
      prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY,
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kSpellCheckConfirmDialogShown,
                                  false,
                                  PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kSpellCheckUseSpellingService,
                                  false,
                                  PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kEnableContinuousSpellcheck,
                                  true,
                                  PrefRegistrySyncable::SYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kEnableAutoSpellCorrect,
                                  false,
                                  PrefRegistrySyncable::SYNCABLE_PREF);
}

bool SpellcheckServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool SpellcheckServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
