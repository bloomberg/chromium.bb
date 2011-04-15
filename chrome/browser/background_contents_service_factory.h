// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BackgroundContentsService;
class Profile;

// Singleton that owns all BackgroundContentsServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated BackgroundContentsService.
class BackgroundContentsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BackgroundContentsService* GetForProfile(Profile* profile);

  static BackgroundContentsServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BackgroundContentsServiceFactory>;

  BackgroundContentsServiceFactory();
  virtual ~BackgroundContentsServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  // Use a separate background contents service for incognito.
  virtual bool ServiceHasOwnInstanceInIncognito() OVERRIDE;
};

#endif  // CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
