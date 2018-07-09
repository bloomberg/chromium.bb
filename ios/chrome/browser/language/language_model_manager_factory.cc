// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/language_model_manager_factory.h"

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/heuristic_language_model.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"

namespace {

std::unique_ptr<language::LanguageModel> BuildDefaultLanguageModelFor(
    ios::ChromeBrowserState* const chrome_state) {
  language::OverrideLanguageModel override_model_mode =
      language::GetOverrideLanguageModel();

  if (override_model_mode == language::OverrideLanguageModel::HEURISTIC) {
    return std::make_unique<language::HeuristicLanguageModel>(
        chrome_state->GetPrefs(),
        GetApplicationContext()->GetApplicationLocale(),
        prefs::kAcceptLanguages, language::prefs::kUserLanguageProfile);
  }

  // language::OverrideLanguageModel::GEO is not supported on iOS yet.

  return std::make_unique<language::BaselineLanguageModel>(
      chrome_state->GetPrefs(), GetApplicationContext()->GetApplicationLocale(),
      prefs::kAcceptLanguages);
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  return base::Singleton<LanguageModelManagerFactory>::get();
}

// static
language::LanguageModelManager* LanguageModelManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* const state) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModelManager",
          BrowserStateDependencyManager::GetInstance()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

std::unique_ptr<KeyedService>
LanguageModelManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* const state) const {
  ios::ChromeBrowserState* const chrome_state =
      ios::ChromeBrowserState::FromBrowserState(state);
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          chrome_state->GetPrefs(),
          GetApplicationContext()->GetApplicationLocale());
  manager->SetDefaultModel(BuildDefaultLanguageModelFor(chrome_state));
  return manager;
}

web::BrowserState* LanguageModelManagerFactory::GetBrowserStateToUse(
    web::BrowserState* const state) const {
  // Use the original profile's language model even in Incognito mode.
  return GetBrowserStateRedirectedInIncognito(state);
}

void LanguageModelManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  if (base::FeatureList::IsEnabled(language::kUseHeuristicLanguageModel)) {
    registry->RegisterDictionaryPref(
        language::prefs::kUserLanguageProfile,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  }
}
