// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/language_model_factory.h"

#include "base/memory/ptr_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/translate/core/browser/language_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
LanguageModelFactory* LanguageModelFactory::GetInstance() {
  return base::Singleton<LanguageModelFactory>::get();
}

// static
translate::LanguageModel* LanguageModelFactory::GetForBrowserState(
    ios::ChromeBrowserState* const state) {
  return static_cast<translate::LanguageModel*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageModelFactory::LanguageModelFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModel",
          BrowserStateDependencyManager::GetInstance()) {}

LanguageModelFactory::~LanguageModelFactory() {}

std::unique_ptr<KeyedService> LanguageModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return base::MakeUnique<translate::LanguageModel>(browser_state->GetPrefs());
}
