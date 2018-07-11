// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/content/browser/geo_language_model.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/heuristic_language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

std::unique_ptr<language::LanguageModel> BuildDefaultLanguageModelFor(
    Profile* const profile) {
  language::OverrideLanguageModel override_model_mode =
      language::GetOverrideLanguageModel();

  if (override_model_mode == language::OverrideLanguageModel::HEURISTIC) {
    return std::make_unique<language::HeuristicLanguageModel>(
        profile->GetPrefs(), g_browser_process->GetApplicationLocale(),
        prefs::kAcceptLanguages, language::prefs::kUserLanguageProfile);
  }

  if (override_model_mode == language::OverrideLanguageModel::GEO) {
    return std::make_unique<language::GeoLanguageModel>(
        language::GeoLanguageProvider::GetInstance());
  }

  return std::make_unique<language::BaselineLanguageModel>(
      profile->GetPrefs(), g_browser_process->GetApplicationLocale(),
      prefs::kAcceptLanguages);
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  return base::Singleton<LanguageModelManagerFactory>::get();
}

// static
language::LanguageModelManager*
LanguageModelManagerFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "LanguageModelManager",
          BrowserContextDependencyManager::GetInstance()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

KeyedService* LanguageModelManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  Profile* const profile = Profile::FromBrowserContext(browser_context);
  language::LanguageModelManager* manager = new language::LanguageModelManager(
      profile->GetPrefs(), g_browser_process->GetApplicationLocale());
  manager->SetDefaultModel(BuildDefaultLanguageModelFor(profile));
  return manager;
}

content::BrowserContext* LanguageModelManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's language model even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void LanguageModelManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  if (language::GetOverrideLanguageModel() ==
      language::OverrideLanguageModel::HEURISTIC) {
    registry->RegisterDictionaryPref(
        language::prefs::kUserLanguageProfile,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  }
}
