// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/network_action_predictor_factory.h"

#include "chrome/browser/autocomplete/network_action_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
NetworkActionPredictor* NetworkActionPredictorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NetworkActionPredictor*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
NetworkActionPredictorFactory* NetworkActionPredictorFactory::GetInstance() {
  return Singleton<NetworkActionPredictorFactory>::get();
}

NetworkActionPredictorFactory::NetworkActionPredictorFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
  // TODO(erg): When HistoryService is PKSFized, uncomment this.
  //  DependsOn(HistoryServiceFactory::GetInstance());
}

NetworkActionPredictorFactory::~NetworkActionPredictorFactory() {}

ProfileKeyedService* NetworkActionPredictorFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new NetworkActionPredictor(profile);
}
