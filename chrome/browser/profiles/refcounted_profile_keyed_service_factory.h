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
 protected:
  RefcountedProfileKeyedServiceFactory(const char* name,
                                       ProfileDependencyManager* manager);
  virtual ~RefcountedProfileKeyedServiceFactory();

  // All subclasses of RefcountedProfileKeyedServiceFactory must return a
  // RefcountedProfileKeyedService instead of just a ProfileKeyedBase.
  virtual RefcountedProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const = 0;

  virtual void Associate(Profile* profile,
                         ProfileKeyedBase* base) OVERRIDE;
  virtual bool GetAssociation(Profile* profile,
                              ProfileKeyedBase** out) const OVERRIDE;
  virtual void ProfileShutdown(Profile* profile) OVERRIDE;
  virtual void ProfileDestroyed(Profile* profile) OVERRIDE;

 private:
  typedef std::map<Profile*, scoped_refptr<RefcountedProfileKeyedService> >
      RefCountedStorage;

  // The mapping between a Profile and its refcounted service.
  RefCountedStorage mapping_;

  DISALLOW_COPY_AND_ASSIGN(RefcountedProfileKeyedServiceFactory);
};

#endif  // CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_H_
