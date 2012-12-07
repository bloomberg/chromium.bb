// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class GoogleNowService;
class Profile;
template<typename Type> struct DefaultSingletonTraits;

// Singleton that owns all GoogleNowService and associates them with
// Profiles.
class GoogleNowServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GoogleNowService* GetForProfile(Profile* profile);
  static GoogleNowServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GoogleNowServiceFactory>;

  GoogleNowServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_FACTORY_H_
