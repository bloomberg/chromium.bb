// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/token_service_factory.h"

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

TokenServiceFactory::TokenServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "TokenService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

TokenServiceFactory::~TokenServiceFactory() {}

// static
TokenService* TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TokenServiceFactory* TokenServiceFactory::GetInstance() {
  return Singleton<TokenServiceFactory>::get();
}

BrowserContextKeyedService* TokenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new TokenService();
}
