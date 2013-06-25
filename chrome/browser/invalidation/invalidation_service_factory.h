// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace syncer {
class Invalidator;
}

class InvalidationFrontend;
class Profile;

namespace invalidation {

// A BrowserContextKeyedServiceFactory to construct InvalidationServices.  The
// implementation of the InvalidationService may be completely different on
// different platforms; this class should help to hide this complexity.  It also
// exposes some factory methods that are useful for setting up tests that rely
// on invalidations.
class InvalidationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // TODO(rlarocque): Re-enable this once InvalidationFrontend can extend
  // BrowserContextKeyedService.
  // static InvalidationFrontend* GetForProfile(Profile* profile);

  static InvalidationServiceFactory* GetInstance();

  static BrowserContextKeyedService* BuildP2PInvalidationServiceFor(
      Profile* profile);
  static BrowserContextKeyedService* BuildTestServiceInstanceFor(
      Profile* profile);

 private:
  friend struct DefaultSingletonTraits<InvalidationServiceFactory>;

  InvalidationServiceFactory();
  virtual ~InvalidationServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  // TODO(rlarocque): Use this class, not InvalidatorStorage, to register
  // for user prefs.
  // virtual void RegisterUserPrefs(PrefRegistrySyncable* registry) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactory);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
