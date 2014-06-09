// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/gcm_driver/gcm_client_factory.h"
#endif

namespace gcm {

// static
GCMProfileService* GCMProfileServiceFactory::GetForProfile(Profile* profile) {
  // GCM is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return NULL;

  return static_cast<GCMProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GCMProfileServiceFactory* GCMProfileServiceFactory::GetInstance() {
  return Singleton<GCMProfileServiceFactory>::get();
}

GCMProfileServiceFactory::GCMProfileServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "GCMProfileService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(LoginUIServiceFactory::GetInstance());
#endif
}

GCMProfileServiceFactory::~GCMProfileServiceFactory() {
}

KeyedService* GCMProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_ANDROID)
  return new GCMProfileService(Profile::FromBrowserContext(context));
#else
  return new GCMProfileService(
      Profile::FromBrowserContext(context),
      scoped_ptr<GCMClientFactory>(new GCMClientFactory));
#endif
}

content::BrowserContext* GCMProfileServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace gcm
