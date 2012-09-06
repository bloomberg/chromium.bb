// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/spellchecker/spellcheck_profile.h"
#include "chrome/common/pref_names.h"
#include "grit/locale_settings.h"

// static
SpellCheckHost* SpellCheckFactory::GetHostForProfile(Profile* profile) {
  return GetInstance()->GetSpellCheckProfile(profile)->GetHost();
}

// static
void SpellCheckFactory::ReinitializeSpellCheckHost(Profile* profile,
                                                   bool force) {
  GetInstance()->GetSpellCheckProfile(profile)->
      ReinitializeSpellCheckHost(force);
}

// static
SpellCheckFactory* SpellCheckFactory::GetInstance() {
  return Singleton<SpellCheckFactory>::get();
}

SpellCheckFactory::SpellCheckFactory()
    : ProfileKeyedServiceFactory("SpellCheckProfile",
                                 ProfileDependencyManager::GetInstance()) {
  // TODO(erg): Uncomment these as they are initialized.
  //
  // DependsOn(RequestContextFactory::GetInstance());
}

SpellCheckFactory::~SpellCheckFactory() {}

SpellCheckProfile* SpellCheckFactory::GetSpellCheckProfile(
    Profile* profile) {
  return static_cast<SpellCheckProfile*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

ProfileKeyedService* SpellCheckFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SpellCheckProfile* spell_check_profile = new SpellCheckProfile(profile);

  // Instantiates Metrics object for spellchecking for use.
  if (g_browser_process->metrics_service() &&
      g_browser_process->metrics_service()->recording_active())
    spell_check_profile->StartRecordingMetrics(
        profile->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck));

  return spell_check_profile;
}

void SpellCheckFactory::RegisterUserPrefs(PrefService* user_prefs) {
  // TODO(estade): IDS_SPELLCHECK_DICTIONARY should be an ASCII string.
  user_prefs->RegisterLocalizedStringPref(prefs::kSpellCheckDictionary,
                                          IDS_SPELLCHECK_DICTIONARY,
                                          PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kSpellCheckConfirmDialogShown,
                                  false,
                                  PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kSpellCheckUseSpellingService,
                                  false,
                                  PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kEnableSpellCheck,
                                  true,
                                  PrefService::SYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kEnableAutoSpellCorrect,
                                  true,
                                  PrefService::UNSYNCABLE_PREF);
}

bool SpellCheckFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool SpellCheckFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
