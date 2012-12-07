// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/google_now/google_now_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/google_now/google_now_service.h"

// static
GoogleNowService* GoogleNowServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<GoogleNowService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GoogleNowServiceFactory* GoogleNowServiceFactory::GetInstance() {
  return Singleton<GoogleNowServiceFactory>::get();
}

GoogleNowServiceFactory::GoogleNowServiceFactory()
    : ProfileKeyedServiceFactory(
          "GoogleNowService", ProfileDependencyManager::GetInstance()) {
}

bool GoogleNowServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

ProfileKeyedService*
GoogleNowServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  GoogleNowService* const google_now_service = new GoogleNowService(profile);
  google_now_service->Init();
  return google_now_service;
}
