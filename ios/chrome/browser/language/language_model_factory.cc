// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/language_model_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/language_model.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"

// static
LanguageModelFactory* LanguageModelFactory::GetInstance() {
  return base::Singleton<LanguageModelFactory>::get();
}

// static
language::LanguageModel* LanguageModelFactory::GetForBrowserState(
    ios::ChromeBrowserState* const state) {
  return static_cast<language::LanguageModel*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageModelFactory::LanguageModelFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModel",
          BrowserStateDependencyManager::GetInstance()) {}

LanguageModelFactory::~LanguageModelFactory() {}

std::unique_ptr<KeyedService> LanguageModelFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  if (base::FeatureList::IsEnabled(language::kUseBaselineLanguageModel)) {
    ios::ChromeBrowserState* chrome_state =
        ios::ChromeBrowserState::FromBrowserState(state);
    return base::MakeUnique<language::BaselineLanguageModel>(
        chrome_state->GetPrefs(),
        GetApplicationContext()->GetApplicationLocale(),
        prefs::kAcceptLanguages);
  }

  return nullptr;
}

web::BrowserState* LanguageModelFactory::GetBrowserStateToUse(
    web::BrowserState* state) const {
  // Use the original profile's language model even in Incognito mode.
  return GetBrowserStateRedirectedInIncognito(state);
}
