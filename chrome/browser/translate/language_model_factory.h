// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_LANGUAGE_MODEL_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_LANGUAGE_MODEL_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace translate {
class LanguageModel;
}

class LanguageModelFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LanguageModelFactory* GetInstance();
  static translate::LanguageModel* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend struct base::DefaultSingletonTraits<LanguageModelFactory>;

  LanguageModelFactory();
  ~LanguageModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(LanguageModelFactory);
};

#endif  // CHROME_BROWSER_TRANSLATE_LANGUAGE_MODEL_FACTORY_H_
