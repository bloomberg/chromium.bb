// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_keyed_base_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;
class ProfileDependencyManager;
class ProfileKeyedService;

// Base class for Factories that take a Profile object and return some service
// on a one-to-one mapping. Each factory that derives from this class *must*
// be a Singleton (only unit tests don't do that). See ThemeServiceFactory as
// an example of how to derive from this class.
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class ProfileKeyedServiceFactory : public ProfileKeyedBaseFactory {
 protected:
  // ProfileKeyedServiceFactories must communicate with a
  // ProfileDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : ProfileKeyedServiceFactory(
  //         "MyService",
  //         ProfileDependencyManager::GetInstance())
  //   {}
  ProfileKeyedServiceFactory(const char* name,
                             ProfileDependencyManager* manager);
  virtual ~ProfileKeyedServiceFactory();

  // Common implementation that maps |profile| to some service object. Deals
  // with incognito profiles per subclass instructions with
  // ServiceRedirectedInIncognito() and ServiceHasOwnInstanceInIncognito().
  // If |create| is true, the service will be created using
  // BuildServiceInstanceFor() if it doesn't already exist.
  ProfileKeyedService* GetServiceForProfile(Profile* profile, bool create);

  // All subclasses of ProfileKeyedServiceFactory must return a
  // ProfileKeyedService instead of just a ProfileKeyedBase.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const = 0;

  // Maps |profile| to |provider| with debug checks to prevent duplication.
  virtual void Associate(Profile* profile, ProfileKeyedBase* base) OVERRIDE;

  // Returns the previously associated |base| for |profile|, or NULL.
  virtual bool GetAssociation(Profile* profile,
                              ProfileKeyedBase** out) const OVERRIDE;

  // A helper object actually listens for notifications about Profile
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, ProfileShutdown() is called on every ServiceFactory and will
  // usually call ProfileKeyedService::Shutdown(), which gives each
  // ProfileKeyedService a chance to remove dependencies on other services that
  // it may be holding.
  //
  // Secondly, ProfileDestroyed() is called on every ServiceFactory and the
  // default implementation removes it from |mapping_| and deletes the pointer.
  virtual void ProfileShutdown(Profile* profile) OVERRIDE;
  virtual void ProfileDestroyed(Profile* profile) OVERRIDE;

 private:
  friend class ProfileDependencyManager;
  friend class ProfileDependencyManagerUnittests;

  // The mapping between a Profile and its service.
  std::map<Profile*, ProfileKeyedService*> mapping_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
