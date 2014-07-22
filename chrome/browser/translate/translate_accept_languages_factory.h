// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace translate {
class TranslateAcceptLanguages;
}

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserContext.
class TranslateAcceptLanguagesFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static translate::TranslateAcceptLanguages* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static TranslateAcceptLanguagesFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TranslateAcceptLanguagesFactory>;

  TranslateAcceptLanguagesFactory();
  virtual ~TranslateAcceptLanguagesFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TranslateAcceptLanguagesFactory);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
