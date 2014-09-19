// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace predictors {

// static
ResourcePrefetchPredictor* ResourcePrefetchPredictorFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<ResourcePrefetchPredictor*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ResourcePrefetchPredictorFactory*
ResourcePrefetchPredictorFactory::GetInstance() {
  return Singleton<ResourcePrefetchPredictorFactory>::get();
}

ResourcePrefetchPredictorFactory::ResourcePrefetchPredictorFactory()
    : BrowserContextKeyedServiceFactory(
        "ResourcePrefetchPredictor",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PredictorDatabaseFactory::GetInstance());
}

ResourcePrefetchPredictorFactory::~ResourcePrefetchPredictorFactory() {}

KeyedService*
    ResourcePrefetchPredictorFactory::BuildServiceInstanceFor(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  ResourcePrefetchPredictorConfig config;
  if (!IsSpeculativeResourcePrefetchingEnabled(profile, &config))
    return NULL;

  return new ResourcePrefetchPredictor(config, profile);
}

}  // namespace predictors
