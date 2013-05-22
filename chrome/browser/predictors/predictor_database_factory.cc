// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database_factory.h"

#include "base/bind.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace predictors {

// static
PredictorDatabase* PredictorDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<PredictorDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PredictorDatabaseFactory* PredictorDatabaseFactory::GetInstance() {
  return Singleton<PredictorDatabaseFactory>::get();
}

PredictorDatabaseFactory::PredictorDatabaseFactory()
    : BrowserContextKeyedServiceFactory(
        "PredictorDatabase", BrowserContextDependencyManager::GetInstance()) {
}

PredictorDatabaseFactory::~PredictorDatabaseFactory() {
}

BrowserContextKeyedService* PredictorDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PredictorDatabase(static_cast<Profile*>(profile));
}

}  // namespace predictors
