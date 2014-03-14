// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace predictors {

class AutocompleteActionPredictor;

// Singleton that owns all AutocompleteActionPredictors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AutocompleteActionPredictor.
class AutocompleteActionPredictorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutocompleteActionPredictor* GetForProfile(Profile* profile);

  static AutocompleteActionPredictorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AutocompleteActionPredictorFactory>;

  AutocompleteActionPredictorFactory();
  virtual ~AutocompleteActionPredictorFactory();

  // BrowserContextKeyedServiceFactory:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteActionPredictorFactory);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_FACTORY_H_
