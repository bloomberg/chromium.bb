// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_factory.h"

#include "chrome/browser/invalidation/invalidation_frontend.h"
#include "chrome/browser/invalidation/invalidation_service_android.h"
#include "chrome/browser/invalidation/p2p_invalidation_service.h"
#include "chrome/browser/invalidation/ticl_invalidation_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"

class TokenService;

namespace invalidation {

// TODO(rlarocque): Re-enable this once InvalidationFrontend can
// extend ProfileKeyedService.
// // static
// InvalidationFrontend* InvalidationServiceFactory::GetForProfile(
//     Profile* profile) {
//   return static_cast<InvalidationFrontend*>(
//       GetInstance()->GetServiceForProfile(profile, true));
// }

// static
InvalidationServiceFactory* InvalidationServiceFactory::GetInstance() {
  return Singleton<InvalidationServiceFactory>::get();
}

InvalidationServiceFactory::InvalidationServiceFactory()
    : ProfileKeyedServiceFactory("InvalidationService",
                                 ProfileDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
#endif
}

InvalidationServiceFactory::~InvalidationServiceFactory() {}

// static
ProfileKeyedService*
InvalidationServiceFactory::BuildP2PInvalidationServiceFor(Profile* profile) {
  return new P2PInvalidationService(profile);
}

ProfileKeyedService* InvalidationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
#if defined(OS_ANDROID)
  InvalidationServiceAndroid* service = new InvalidationServiceAndroid(profile);
  return service;
#else
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile);

  TiclInvalidationService* service = new TiclInvalidationService(
      signin_manager, token_service, profile);
  service->Init();
  return service;
#endif
}

}  // namespace invalidation
