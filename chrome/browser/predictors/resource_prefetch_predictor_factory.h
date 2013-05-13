// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace predictors {

class ResourcePrefetchPredictor;

class ResourcePrefetchPredictorFactory : public ProfileKeyedServiceFactory {
 public:
  static ResourcePrefetchPredictor* GetForProfile(Profile* profile);
  static ResourcePrefetchPredictorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ResourcePrefetchPredictorFactory>;

  ResourcePrefetchPredictorFactory();
  virtual ~ResourcePrefetchPredictorFactory();

  // RefcountedProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorFactory);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_FACTORY_H_
