// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NetworkActionPredictor;

// Singleton that owns all NetworkActionPredictors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated NetworkActionPredictor.
class NetworkActionPredictorFactory : public ProfileKeyedServiceFactory {
 public:
  static NetworkActionPredictor* GetForProfile(Profile* profile);

  static NetworkActionPredictorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkActionPredictorFactory>;

  NetworkActionPredictorFactory();
  virtual ~NetworkActionPredictorFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkActionPredictorFactory);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_FACTORY_H_
