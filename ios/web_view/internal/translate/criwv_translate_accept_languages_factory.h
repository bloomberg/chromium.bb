// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include <memory>
#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace translate {
class TranslateAcceptLanguages;
}

namespace ios_web_view {

class CRIWVBrowserState;

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserState.
class CRIWVTranslateAcceptLanguagesFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static translate::TranslateAcceptLanguages* GetForBrowserState(
      CRIWVBrowserState* browser_state);
  static CRIWVTranslateAcceptLanguagesFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      CRIWVTranslateAcceptLanguagesFactory>;

  CRIWVTranslateAcceptLanguagesFactory();
  ~CRIWVTranslateAcceptLanguagesFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CRIWVTranslateAcceptLanguagesFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
