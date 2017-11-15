// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/language/web_view_language_model_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/heuristic_language_model.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/pref_names.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewLanguageModelFactory* WebViewLanguageModelFactory::GetInstance() {
  return base::Singleton<WebViewLanguageModelFactory>::get();
}

// static
language::LanguageModel* WebViewLanguageModelFactory::GetForBrowserState(
    WebViewBrowserState* const state) {
  return static_cast<language::LanguageModel*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

WebViewLanguageModelFactory::WebViewLanguageModelFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModel",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewLanguageModelFactory::BuildServiceInstanceFor(
    web::BrowserState* const context) const {
  WebViewBrowserState* const web_view_browser_state =
      WebViewBrowserState::FromBrowserState(context);

  if (base::FeatureList::IsEnabled(language::kUseHeuristicLanguageModel)) {
    return base::MakeUnique<language::HeuristicLanguageModel>(
        web_view_browser_state->GetPrefs(),
        ApplicationContext::GetInstance()->GetApplicationLocale(),
        prefs::kAcceptLanguages, language::prefs::kUserLanguageProfile);
  }

  return base::MakeUnique<language::BaselineLanguageModel>(
      web_view_browser_state->GetPrefs(),
      ApplicationContext::GetInstance()->GetApplicationLocale(),
      prefs::kAcceptLanguages);
}

void WebViewLanguageModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  if (base::FeatureList::IsEnabled(language::kUseHeuristicLanguageModel)) {
    registry->RegisterDictionaryPref(
        language::prefs::kUserLanguageProfile,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  }
}

}  // namespace ios_web_view
