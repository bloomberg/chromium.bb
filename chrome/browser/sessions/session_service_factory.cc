// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sessions/session_service.h"

// static
SessionService* SessionServiceFactory::GetForProfile(Profile* profile) {
#if defined(OS_ANDROID)
  // For Android we do not store sessions in the SessionService.
  return NULL;
#else
  return static_cast<SessionService*>(
      GetInstance()->GetServiceForProfile(profile, true));
#endif
}

// static
SessionService* SessionServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
#if defined(OS_ANDROID)
  // For Android we do not store sessions in the SessionService.
  return NULL;
#else
  return static_cast<SessionService*>(
      GetInstance()->GetServiceForProfile(profile, false));
#endif
}

// static
void SessionServiceFactory::ShutdownForProfile(Profile* profile) {
  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  SessionServiceFactory* factory = GetInstance();
  factory->GetServiceForProfile(profile, true);

  // Shut down and remove the reference to the session service, and replace it
  // with an explicit NULL to prevent it being recreated on the next access.
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
  factory->Associate(profile, NULL);
}

SessionServiceFactory* SessionServiceFactory::GetInstance() {
  return Singleton<SessionServiceFactory>::get();
}

SessionServiceFactory::SessionServiceFactory()
    : ProfileKeyedServiceFactory("SessionService",
                                 ProfileDependencyManager::GetInstance()) {
}

SessionServiceFactory::~SessionServiceFactory() {
}

ProfileKeyedService* SessionServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SessionService* service = NULL;
  service = new SessionService(profile);
  service->ResetFromCurrentBrowsers();
  return service;
}

bool SessionServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool SessionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
