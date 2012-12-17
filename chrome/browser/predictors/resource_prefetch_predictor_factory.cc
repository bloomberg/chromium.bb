// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace predictors {

// static
ResourcePrefetchPredictor* ResourcePrefetchPredictorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ResourcePrefetchPredictor*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ResourcePrefetchPredictorFactory*
ResourcePrefetchPredictorFactory::GetInstance() {
  return Singleton<ResourcePrefetchPredictorFactory>::get();
}

ResourcePrefetchPredictorFactory::ResourcePrefetchPredictorFactory()
    : ProfileKeyedServiceFactory("ResourcePrefetchPredictor",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PredictorDatabaseFactory::GetInstance());
}

ResourcePrefetchPredictorFactory::~ResourcePrefetchPredictorFactory() {}

ProfileKeyedService*
    ResourcePrefetchPredictorFactory::BuildServiceInstanceFor(
        Profile* profile) const {
  ResourcePrefetchPredictorConfig config;
  if (!IsSpeculativeResourcePrefetchingEnabled(profile, &config))
    return NULL;

  return new ResourcePrefetchPredictor(config, profile);
}

}  // namespace predictors
