// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_MODE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_MODE_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BackgroundModeManager;
class Profile;

// Singleton that owns all BackgroundModeManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated BackgroundModeManager.
class BackgroundModeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static BackgroundModeManager* GetForProfile(Profile* profile);

  static BackgroundModeManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BackgroundModeManagerFactory>;

  BackgroundModeManagerFactory();
  virtual ~BackgroundModeManagerFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_BACKGROUND_MODE_MANAGER_FACTORY_H_
