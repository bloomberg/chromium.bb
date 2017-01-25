// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/translate/criwv_translate_accept_languages_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "ios/web_view/internal/criwv_browser_state.h"
#include "ios/web_view/internal/pref_names.h"

namespace {

// TranslateAcceptLanguagesService is a thin container for
// TranslateAcceptLanguages to enable associating it with a BrowserState.
class TranslateAcceptLanguagesService : public KeyedService {
 public:
  explicit TranslateAcceptLanguagesService(PrefService* prefs);
  ~TranslateAcceptLanguagesService() override;

  // Returns the associated TranslateAcceptLanguages.
  translate::TranslateAcceptLanguages& accept_languages() {
    return accept_languages_;
  }

 private:
  translate::TranslateAcceptLanguages accept_languages_;

  DISALLOW_COPY_AND_ASSIGN(TranslateAcceptLanguagesService);
};

TranslateAcceptLanguagesService::TranslateAcceptLanguagesService(
    PrefService* prefs)
    : accept_languages_(prefs, prefs::kAcceptLanguages) {}

TranslateAcceptLanguagesService::~TranslateAcceptLanguagesService() {}

}  // namespace

namespace ios_web_view {

// static
CRIWVTranslateAcceptLanguagesFactory*
CRIWVTranslateAcceptLanguagesFactory::GetInstance() {
  return base::Singleton<CRIWVTranslateAcceptLanguagesFactory>::get();
}

// static
translate::TranslateAcceptLanguages*
CRIWVTranslateAcceptLanguagesFactory::GetForBrowserState(
    CRIWVBrowserState* state) {
  TranslateAcceptLanguagesService* service =
      static_cast<TranslateAcceptLanguagesService*>(
          GetInstance()->GetServiceForBrowserState(state, true));
  return &service->accept_languages();
}

CRIWVTranslateAcceptLanguagesFactory::CRIWVTranslateAcceptLanguagesFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateAcceptLanguagesService",
          BrowserStateDependencyManager::GetInstance()) {}

CRIWVTranslateAcceptLanguagesFactory::~CRIWVTranslateAcceptLanguagesFactory() {}

std::unique_ptr<KeyedService>
CRIWVTranslateAcceptLanguagesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  CRIWVBrowserState* criwv_browser_state =
      CRIWVBrowserState::FromBrowserState(context);
  return base::MakeUnique<TranslateAcceptLanguagesService>(
      criwv_browser_state->GetPrefs());
}

}  // namespace ios_web_view
