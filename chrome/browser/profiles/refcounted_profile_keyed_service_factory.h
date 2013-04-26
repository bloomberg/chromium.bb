// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"

class Profile;
class RefcountedProfileKeyedService;

// A specialized ProfileKeyedServiceFactory that manages a
// RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedProfileKeyedService should note that we guarantee
// that ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class RefcountedProfileKeyedServiceFactory : public ProfileKeyedBaseFactory {
 public:
  // A function that supplies the instance of a ProfileKeyedService for a given
  // Profile. This is used primarily for testing, where we want to feed a
  // specific mock into the PKSF system.
  typedef scoped_refptr<RefcountedProfileKeyedService>
      (*FactoryFunction)(content::BrowserContext* profile);

  // Associates |factory| with |profile| so that |factory| is used to create
  // the ProfileKeyedService when requested.  |factory| can be NULL to signal
  // that ProfileKeyedService should be NULL. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* profile,
                         FactoryFunction factory);

  // Associates |factory| with |profile| and immediately returns the created
  // ProfileKeyedService. Since the factory will be used immediately, it may
  // not be NULL.
  scoped_refptr<RefcountedProfileKeyedService> SetTestingFactoryAndUse(
      content::BrowserContext* profile,
      FactoryFunction factory);

 protected:
  RefcountedProfileKeyedServiceFactory(const char* name,
                                       ProfileDependencyManager* manager);
  virtual ~RefcountedProfileKeyedServiceFactory();

  scoped_refptr<RefcountedProfileKeyedService> GetServiceForProfile(
      content::BrowserContext* profile,
      bool create);

  // Maps |profile| to |service| with debug checks to prevent duplication.
  void Associate(content::BrowserContext* profile,
                 const scoped_refptr<RefcountedProfileKeyedService>& service);

  // All subclasses of RefcountedProfileKeyedServiceFactory must return a
  // RefcountedProfileKeyedService instead of just a ProfileKeyedBase.
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* profile) const = 0;

  virtual void ProfileShutdown(content::BrowserContext* profile) OVERRIDE;
  virtual void ProfileDestroyed(content::BrowserContext* profile) OVERRIDE;
  virtual void SetEmptyTestingFactory(
      content::BrowserContext* profile) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* profile) OVERRIDE;

 private:
  typedef std::map<content::BrowserContext*,
                   scoped_refptr<RefcountedProfileKeyedService> >
      RefCountedStorage;
  typedef std::map<content::BrowserContext*,
                   FactoryFunction> ProfileOverriddenFunctions;

  // The mapping between a Profile and its refcounted service.
  RefCountedStorage mapping_;

  // The mapping between a Profile and its overridden FactoryFunction.
  ProfileOverriddenFunctions factories_;

  DISALLOW_COPY_AND_ASSIGN(RefcountedProfileKeyedServiceFactory);
};

#endif  // CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_
