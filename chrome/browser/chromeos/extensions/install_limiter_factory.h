// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class InstallLimiter;

// Singleton that owns all InstallLimiter and associates them with profiles.
class InstallLimiterFactory : public ProfileKeyedServiceFactory {
 public:
  static InstallLimiter* GetForProfile(Profile* profile);

  static InstallLimiterFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<InstallLimiterFactory>;

  InstallLimiterFactory();
  virtual ~InstallLimiterFactory();

  // ProfileKeyedServiceFactory overrides:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_
