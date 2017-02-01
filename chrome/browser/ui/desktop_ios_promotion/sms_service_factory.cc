// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/sms_service_factory.h"

#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/desktop_ios_promotion/sms_service.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/url_request/url_request_context_getter.h"

// static
SMSServiceFactory* SMSServiceFactory::GetInstance() {
  return base::Singleton<SMSServiceFactory>::get();
}

// static
SMSService* SMSServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SMSService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* SMSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SMSService(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      profile->GetRequestContext());
}

SMSServiceFactory::SMSServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SMSServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

SMSServiceFactory::~SMSServiceFactory() {}
