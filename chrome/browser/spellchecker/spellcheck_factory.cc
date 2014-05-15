// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/render_process_host.h"
#include "grit/locale_settings.h"

// static
SpellcheckService* SpellcheckServiceFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<SpellcheckService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SpellcheckService* SpellcheckServiceFactory::GetForRenderProcessId(
    int render_process_id) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return NULL;
  content::BrowserContext* context = host->GetBrowserContext();
  if (!context)
    return NULL;
  return GetForContext(context);
}

// static
SpellcheckServiceFactory* SpellcheckServiceFactory::GetInstance() {
  return Singleton<SpellcheckServiceFactory>::get();
}

SpellcheckServiceFactory::SpellcheckServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "SpellcheckService",
        BrowserContextDependencyManager::GetInstance()) {
  // TODO(erg): Uncomment these as they are initialized.
  // DependsOn(RequestContextFactory::GetInstance());
}

SpellcheckServiceFactory::~SpellcheckServiceFactory() {}

KeyedService* SpellcheckServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Many variables are initialized from the |context| in the SpellcheckService.
  SpellcheckService* spellcheck = new SpellcheckService(context);

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  DCHECK(prefs);

  // Instantiates Metrics object for spellchecking for use.
  spellcheck->StartRecordingMetrics(
      prefs->GetBoolean(prefs::kEnableContinuousSpellcheck));

  return spellcheck;
}

void SpellcheckServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  // TODO(estade): IDS_SPELLCHECK_DICTIONARY should be an ASCII string.
  user_prefs->RegisterLocalizedStringPref(
      prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(
      prefs::kSpellCheckUseSpellingService,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(
      prefs::kEnableContinuousSpellcheck,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(
      prefs::kEnableAutoSpellCorrect,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

content::BrowserContext* SpellcheckServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SpellcheckServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
