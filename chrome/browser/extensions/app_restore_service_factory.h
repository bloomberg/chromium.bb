// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class AppRestoreService;

// Singleton that owns all AppRestoreServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppRestoreService.
class AppRestoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AppRestoreService* GetForProfile(Profile* profile);

  static void ResetForProfile(Profile* profile);

  static AppRestoreServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppRestoreServiceFactory>;

  AppRestoreServiceFactory();
  virtual ~AppRestoreServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_RESTORE_SERVICE_FACTORY_H_
