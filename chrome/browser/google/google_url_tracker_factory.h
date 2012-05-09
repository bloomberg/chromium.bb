// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class GoogleURLTracker;
class Profile;

// Singleton that owns all GoogleURLTrackers and associates them with Profiles.
class GoogleURLTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the GoogleURLTracker for |profile|.  This may return NULL for a
  // testing profile.
  static GoogleURLTracker* GetForProfile(Profile* profile);

  static GoogleURLTrackerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GoogleURLTrackerFactory>;

  GoogleURLTrackerFactory();
  virtual ~GoogleURLTrackerFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerFactory);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_FACTORY_H_
