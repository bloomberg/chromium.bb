// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace syncer {
class Invalidator;
}

class Profile;

namespace invalidation {

class FakeInvalidationService;
class InvalidationService;
class P2PInvalidationService;

// A BrowserContextKeyedServiceFactory to construct InvalidationServices.  The
// implementation of the InvalidationService may be completely different on
// different platforms; this class should help to hide this complexity.  It also
// exposes some factory methods that are useful for setting up tests that rely
// on invalidations.
class InvalidationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the InvalidationService for the given |profile|, lazily creating
  // one first if required. If |profile| does not support invalidation
  // (Chrome OS login profile, Chrome OS guest), returns NULL instead.
  static InvalidationService* GetForProfile(Profile* profile);

  static InvalidationServiceFactory* GetInstance();

  // Switches service creation to go through |testing_factory| for all browser
  // contexts.
  void RegisterTestingFactory(TestingFactoryFunction testing_factory);

 private:
  friend class InvalidationServiceFactoryTestBase;
  friend struct DefaultSingletonTraits<InvalidationServiceFactory>;

  InvalidationServiceFactory();
  virtual ~InvalidationServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;

  TestingFactoryFunction testing_factory_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactory);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_H_
