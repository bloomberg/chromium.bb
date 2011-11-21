// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sessions/session_service.h"

class Profile;

// Singleton that owns all SessionServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SessionService.
class SessionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the session service for |profile|. This may return NULL. If this
  // profile supports a session service (it isn't incognito), and the session
  // service hasn't yet been created, this forces creation of the session
  // service.
  //
  // This returns NULL in two situations: the profile is incognito, or the
  // session service has been explicitly shutdown (browser is exiting). Callers
  // should always check the return value for NULL.
  static SessionService* GetForProfile(Profile* profile);

  // Returns the session service for |profile|, but do not create it if it
  // doesn't exist.
  static SessionService* GetForProfileIfExisting(Profile* profile);

  // If |profile| has a session service, it is shut down. To properly record the
  // current state this forces creation of the session service, then shuts it
  // down.
  static void ShutdownForProfile(Profile* profile);

#if defined(UNIT_TEST)
  // For test use: force setting of the session service for a given profile.
  // This will delete a previous session service for this profile if it exists.
  static void SetForTestProfile(Profile* profile, SessionService* service) {
    GetInstance()->ProfileShutdown(profile);
    GetInstance()->ProfileDestroyed(profile);
    GetInstance()->Associate(profile, service);
  }
#endif

  static SessionServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SessionServiceFactory>;

  SessionServiceFactory();
  virtual ~SessionServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_FACTORY_H_
