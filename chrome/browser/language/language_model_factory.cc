// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/baseline_language_model.h"

// static
LanguageModelFactory* LanguageModelFactory::GetInstance() {
  return base::Singleton<LanguageModelFactory>::get();
}

// static
language::LanguageModel* LanguageModelFactory::GetForBrowserContext(
    content::BrowserContext* const browser_context) {
  return static_cast<language::LanguageModel*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

LanguageModelFactory::LanguageModelFactory()
    : BrowserContextKeyedServiceFactory(
          "LanguageModel",
          BrowserContextDependencyManager::GetInstance()) {}

LanguageModelFactory::~LanguageModelFactory() {}

KeyedService* LanguageModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* const browser_context) const {
  if (base::FeatureList::IsEnabled(language::kUseBaselineLanguageModel)) {
    Profile* const profile = Profile::FromBrowserContext(browser_context);
    return new language::BaselineLanguageModel(
        profile->GetPrefs(), g_browser_process->GetApplicationLocale(),
        prefs::kAcceptLanguages);
  }

  return nullptr;
}

content::BrowserContext* LanguageModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's language model even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
