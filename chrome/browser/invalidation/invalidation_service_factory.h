// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace syncer {
class Invalidator;
}

class Profile;

namespace invalidation {

class InvalidationService;
class P2PInvalidationService;
class FakeInvalidationService;

// A BrowserContextKeyedServiceFactory to construct InvalidationServices.  The
// implementation of the InvalidationService may be completely different on
// different platforms; this class should help to hide this complexity.  It also
// exposes some factory methods that are useful for setting up tests that rely
// on invalidations.
class InvalidationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static InvalidationService* GetForProfile(Profile* profile);

  static InvalidationServiceFactory* GetInstance();

  // A helper function to set this factory to return FakeInvalidationServices
  // instead of the default InvalidationService objects.
  void SetBuildOnlyFakeInvalidatorsForTest(bool test_mode_enabled);

  // These helper functions to set the factory to build a test-only type of
  // invalidator and return the instance immeidately.
  P2PInvalidationService* BuildAndUseP2PInvalidationServiceForTest(
      content::BrowserContext* context);

 private:
  friend struct DefaultSingletonTraits<InvalidationServiceFactory>;

  InvalidationServiceFactory();
  virtual ~InvalidationServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;

  // If true, this factory will return only FakeInvalidationService instances.
  bool build_fake_invalidators_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactory);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
