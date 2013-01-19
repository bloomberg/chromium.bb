// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ManagedUserService;

class ManagedUserServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ManagedUserService* GetForProfile(Profile* profile);

  static ManagedUserServiceFactory* GetInstance();

  // Used to create instances for testing.
  static ProfileKeyedService* BuildInstanceFor(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<ManagedUserServiceFactory>;

  ManagedUserServiceFactory();
  virtual ~ManagedUserServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_
