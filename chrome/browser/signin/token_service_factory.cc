// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_service_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/token_service.h"

TokenServiceFactory::TokenServiceFactory()
    : ProfileKeyedServiceFactory("TokenService",
                                 ProfileDependencyManager::GetInstance()) {
  // TODO(rlp): TokenService depends on WebDataService - when this is
  // converted to the ProfileKeyedService framework, uncomment this dependency.
  // DependsOn(WebDataServiceFactory::GetInstance());
}

TokenServiceFactory::~TokenServiceFactory() {}

// static
TokenService* TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TokenService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
TokenServiceFactory* TokenServiceFactory::GetInstance() {
  return Singleton<TokenServiceFactory>::get();
}

ProfileKeyedService* TokenServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TokenService();
}
