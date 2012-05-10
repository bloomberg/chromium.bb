// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database_factory.h"

#include "base/bind.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace predictors {

// static
PredictorDatabase* PredictorDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<PredictorDatabase*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PredictorDatabaseFactory* PredictorDatabaseFactory::GetInstance() {
  return Singleton<PredictorDatabaseFactory>::get();
}

PredictorDatabaseFactory::PredictorDatabaseFactory()
    : ProfileKeyedServiceFactory(
        "PredictorDatabase", ProfileDependencyManager::GetInstance()) {
}

PredictorDatabaseFactory::~PredictorDatabaseFactory() {
}

ProfileKeyedService* PredictorDatabaseFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PredictorDatabase(profile);
}

}  // namespace predictors
